#include "stdafx.h"

#if  defined DXR_ENABLED

#include "ModelLoader.h"
#include "DXR.h"
#include "Renderables/Model.h"
#include "Resources/Graphics.h"
#include "Renderables/Mesh.h"
#include "Resources/Buffer.h"
#include <algorithm>
#include <vector>

using namespace std;

namespace DXR
{
	void CreateAccelerationStructure()
	{
		ID3D12Device5* device = (ID3D12Device5*) Graphics::Context.m_device;
		ID3D12GraphicsCommandList4* commandList = (ID3D12GraphicsCommandList4*)Graphics::Context.m_commandList;

		Scene* scene = Graphics::Context.m_scene;

		//Create BLAS
		{
			for (Model* model : scene->GetModels())
			{
				Mesh* mesh = model->GetMesh();

				D3D12_RAYTRACING_GEOMETRY_DESC desc;
				desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
				desc.Triangles.VertexBuffer.StartAddress = mesh->GetVertexBuffer()->GetGPUVirtualAddress();
				desc.Triangles.VertexBuffer.StrideInBytes = mesh->GetVertexBufferView().StrideInBytes;
				desc.Triangles.VertexCount = static_cast<UINT>(mesh->GetVertices().size());
				desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				desc.Triangles.IndexBuffer = mesh->GetIndexBuffer()->GetGPUVirtualAddress();
				desc.Triangles.IndexFormat = mesh->GetIndexBufferView().Format;
				desc.Triangles.IndexCount = static_cast<UINT>(mesh->GetIndices().size());
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

				ASPreBuildInfo.ScratchDataSizeInBytes = Align(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
				ASPreBuildInfo.ResultDataMaxSizeInBytes = Align(ASPreBuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

				// Create the BLAS scratch buffer
				Buffer::Description blasdesc;
				blasdesc.m_alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
				blasdesc.m_elementSize = ASPreBuildInfo.ScratchDataSizeInBytes;
				blasdesc.m_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				blasdesc.m_descriptorType = Buffer::DescriptorType::UAV;

				Buffer* blasScratchBuffer = new Buffer(blasdesc, L"BLAS Scratch Buffer");

				// Create the BLAS buffer
				blasdesc.m_elementSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;
				blasdesc.m_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

				Buffer* blasBuffer = new Buffer(blasdesc, L"BLAS Buffer");

				// Describe and build the bottom level acceleration structure
				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
				buildDesc.Inputs = ASInputs;
				buildDesc.ScratchAccelerationStructureData = blasScratchBuffer->GetResource()->GetGPUVirtualAddress();
				buildDesc.DestAccelerationStructureData = blasBuffer->GetResource()->GetGPUVirtualAddress();

				commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

				// Wait for the BLAS build to complete
				D3D12_RESOURCE_BARRIER uavBarrier;
				uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				uavBarrier.UAV.pResource = blasBuffer->GetResource();
				uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				commandList->ResourceBarrier(1, &uavBarrier);

				model->SetBlasBuffer(blasBuffer);

				delete blasScratchBuffer;
			}
		}

		//create TLAS
		{
			for (ModelInstance* modelInstance : scene->GetModelInstances())
			{
				Model* model = modelInstance->GetModel();

				// Describe the TLAS geometry instance(s)
				D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
				instanceDesc.InstanceID = 0;																		// This value is exposed to shaders as SV_InstanceID
				instanceDesc.InstanceContributionToHitGroupIndex = 0;
				instanceDesc.InstanceMask = 0xFF;
				instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;		// identity transform
				instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
				instanceDesc.AccelerationStructure = model->GetBlasBuffer()->GetResource()->GetGPUVirtualAddress();// dxr.BLAS.pResult->GetGPUVirtualAddress();

				// Create the TLAS instance buffer
				//D3D12BufferCreateInfo instanceBufferInfo;
				//instanceBufferInfo.size = sizeof(instanceDesc);
				//instanceBufferInfo.heapType = D3D12_HEAP_TYPE_UPLOAD;
				//instanceBufferInfo.flags = D3D12_RESOURCE_FLAG_NONE;
				//instanceBufferInfo.state = D3D12_RESOURCE_STATE_GENERIC_READ;
				//D3DResources::Create_Buffer(d3d, instanceBufferInfo, &dxr.TLAS.pInstanceDesc);

				Buffer::Description desc;
				desc.m_elementSize = sizeof(instanceDesc);
				desc.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
				desc.m_resourceFlags = D3D12_RESOURCE_FLAG_NONE;
				desc.m_heapType = D3D12_HEAP_TYPE_UPLOAD;

				Buffer* instanceDescriptionBuffer = new Buffer(desc, L"Instance Description Buffer");

				// Copy the instance data to the buffer
				UINT8* data;
				instanceDescriptionBuffer->GetResource()->Map(0, nullptr, (void**)&data);
				memcpy(data, &instanceDesc, sizeof(instanceDesc));
				instanceDescriptionBuffer->GetResource()->Unmap(0, nullptr);

				D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

				// Get the size requirements for the TLAS buffers
				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
				ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
				ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
				ASInputs.InstanceDescs = instanceDescriptionBuffer->GetResource()->GetGPUVirtualAddress();
				ASInputs.NumDescs = 1;
				ASInputs.Flags = buildFlags;

				D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
				device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

				ASPreBuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
				ASPreBuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

				// Set TLAS size
				//dxr.tlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

				// Create TLAS scratch buffer
				//D3D12BufferCreateInfo bufferInfo(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				//bufferInfo.alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
				//D3DResources::Create_Buffer(d3d, bufferInfo, &dxr.TLAS.pScratch);

				Buffer::Description tlasDesc;
				tlasDesc.m_alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
				tlasDesc.m_elementSize = ASPreBuildInfo.ScratchDataSizeInBytes;
				tlasDesc.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
				tlasDesc.m_resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				tlasDesc.m_descriptorType = Buffer::DescriptorType::UAV;
			
				Buffer* tlasScratchBuffer = new Buffer(tlasDesc, L"TLAS Scratch Buffer");

				// Create the TLAS buffer
				tlasDesc.m_elementSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;
				tlasDesc.m_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

				Buffer* tlasBuffer = new Buffer(tlasDesc, L"TLAS Buffer");

				// Describe and build the TLAS
				D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
				buildDesc.Inputs = ASInputs;
				buildDesc.ScratchAccelerationStructureData = tlasScratchBuffer->GetResource()->GetGPUVirtualAddress();
				buildDesc.DestAccelerationStructureData = tlasBuffer->GetResource()->GetGPUVirtualAddress();

				commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

				// Wait for the TLAS build to complete
				D3D12_RESOURCE_BARRIER uavBarrier;
				uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				uavBarrier.UAV.pResource = tlasBuffer->GetResource();
				uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				commandList->ResourceBarrier(1, &uavBarrier);

				delete tlasScratchBuffer;

				scene->SetTLASBuffer(tlasBuffer);
			}
		}
	}

	void CreateShaders()
	{

	}

	void InitResources()
	{
		CreateAccelerationStructure();
	}
}

#endif // DXR