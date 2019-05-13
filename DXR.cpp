#include "stdafx.h"

#if defined DXR

#include "DXR.h"
#include "Renderables/Mesh.h"
#include "Renderables/Model.h"
#include "Resources/Buffer.h"
#include <algorithm>
#include <vector>

using namespace std;

namespace DXR
{
	void CreateAccelerationStructures()
	{
		ID3D12Device* device = Graphics::Context.m_device;

		//Create BLAS
		{
			D3D12_RAYTRACING_GEOMETRY_DESC desc;
			desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			desc.Triangles.VertexBuffer.StartAddress = resources.vertexBuffer->GetGPUVirtualAddress();
			desc.Triangles.VertexBuffer.StrideInBytes = resources.vertexBufferView.StrideInBytes;
			desc.Triangles.VertexCount = static_cast<UINT>(model.vertices.size());
			desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			desc.Triangles.IndexBuffer = resources.indexBuffer->GetGPUVirtualAddress();
			desc.Triangles.IndexFormat = resources.indexBufferView.Format;
			desc.Triangles.IndexCount = static_cast<UINT>(model.indices.size());
			desc.Triangles.Transform3x4 = 0;
			desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

			// Get the size requirements for the BLAS buffers
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
			ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			ASInputs.pGeometryDescs = &desc;
			ASInputs.NumDescs = 1;
			ASInputs.Flags = buildFlags;

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
			device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

			ASPreBuildInfo.ScratchDataSizeInBytes = align( ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
			ASPreBuildInfo.ResultDataMaxSizeInBytes = align( ASPreBuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

			// Create the BLAS scratch buffer
			D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
			
			Buffer* BLASBuffer = new Buffer()
			D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.BLAS.pScratch);

			// Create the BLAS buffer
			bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
			bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
			D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.BLAS.pResult);

			// Describe and build the bottom level acceleration structure
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
			buildDesc.Inputs = ASInputs;
			buildDesc.ScratchAccelerationStructureData = dxr.BLAS.pScratch->GetGPUVirtualAddress();
			buildDesc.DestAccelerationStructureData = dxr.BLAS.pResult->GetGPUVirtualAddress();

			d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

			// Wait for the BLAS build to complete
			D3D12_RESOURCE_BARRIER uavBarrier;
			uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			uavBarrier.UAV.pResource = dxr.BLAS.pResult;
			uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			d3d.cmdList->ResourceBarrier(1, &uavBarrier);
		}

		//create TLAS
		{
			// Describe the TLAS geometry instance(s)
			D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
			instanceDesc.InstanceID = 0;																		// This value is exposed to shaders as SV_InstanceID
			instanceDesc.InstanceContributionToHitGroupIndex = 0;
			instanceDesc.InstanceMask = 0xFF;
			instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;		// identity transform
			instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			instanceDesc.AccelerationStructure = dxr.BLAS.pResult->GetGPUVirtualAddress();

			// Create the TLAS instance buffer
			D3D12BufferCreateInfo instanceBufferInfo;
			instanceBufferInfo.size = sizeof(instanceDesc);
			instanceBufferInfo.heapType = D3D12_HEAP_TYPE_UPLOAD;
			instanceBufferInfo.flags = D3D12_RESOURCE_FLAG_NONE;
			instanceBufferInfo.state = D3D12_RESOURCE_STATE_GENERIC_READ;
			D3DResources::Create_Buffer(d3d, instanceBufferInfo, &dxr.TLAS.pInstanceDesc);

			// Copy the instance data to the buffer
			UINT8* pData;
			dxr.TLAS.pInstanceDesc->Map(0, nullptr, (void**)&pData);
			memcpy(pData, &instanceDesc, sizeof(instanceDesc));
			dxr.TLAS.pInstanceDesc->Unmap(0, nullptr);

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

			// Get the size requirements for the TLAS buffers
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
			ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
			ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			ASInputs.InstanceDescs = dxr.TLAS.pInstanceDesc->GetGPUVirtualAddress();
			ASInputs.NumDescs = 1;
			ASInputs.Flags = buildFlags;

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
			d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

			ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
			ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

			// Set TLAS size
			dxr.tlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

			// Create TLAS scratch buffer
			D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
			D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.TLAS.pScratch);

			// Create the TLAS buffer
			bufferInfo.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
			bufferInfo.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
			D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.TLAS.pResult);

			// Describe and build the TLAS
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
			buildDesc.Inputs = ASInputs;
			buildDesc.ScratchAccelerationStructureData = dxr.TLAS.pScratch->GetGPUVirtualAddress();
			buildDesc.DestAccelerationStructureData = dxr.TLAS.pResult->GetGPUVirtualAddress();

			d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

			// Wait for the TLAS build to complete
			D3D12_RESOURCE_BARRIER uavBarrier;
			uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			uavBarrier.UAV.pResource = dxr.TLAS.pResult;
			uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			d3d.cmdList->ResourceBarrier(1, &uavBarrier);
		}
	}


	void InitResources()
	{

	}
}

#endif // DXR