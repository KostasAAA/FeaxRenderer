#include "stdafx.h"

#if defined DXR_ENABLED

#include "dxc/dxcapi.use.h"
#include "ModelLoader.h"
#include "DXR.h"
#include "Renderables/Model.h"
#include "Resources/Graphics.h"
#include "Renderables/Mesh.h"
#include "Resources/RootSignature.h"
#include "Resources/Buffer.h"
#include "Resources/Rendertarget.h"
#include "ShaderCompiler.h"
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
				blasdesc.m_size = (uint)ASPreBuildInfo.ScratchDataSizeInBytes;
				blasdesc.m_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				blasdesc.m_descriptorType = Buffer::DescriptorType::UAV;

				Buffer* blasScratchBuffer = new Buffer(blasdesc, L"BLAS Scratch Buffer");

				// Create the BLAS buffer
				blasdesc.m_elementSize = (uint)ASPreBuildInfo.ResultDataMaxSizeInBytes;
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
			D3D12_RAYTRACING_INSTANCE_DESC* instanceDescriptions = new D3D12_RAYTRACING_INSTANCE_DESC[scene->GetModelInstances().size()];

			int noofInstances = 0;
			for (ModelInstance* modelInstance : scene->GetModelInstances())
			{
				Model* model = modelInstance->GetModel();

				D3D12_RAYTRACING_INSTANCE_DESC& instanceDesc = instanceDescriptions[noofInstances];
				// Describe the TLAS geometry instance(s)
				instanceDesc.InstanceID = noofInstances;																		// This value is exposed to shaders as SV_InstanceID
				instanceDesc.InstanceContributionToHitGroupIndex = 0;
				instanceDesc.InstanceMask = 0xFF;
				instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;		// identity transform
				instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
				instanceDesc.AccelerationStructure = model->GetBlasBuffer()->GetResource()->GetGPUVirtualAddress();// dxr.BLAS.pResult->GetGPUVirtualAddress();

				noofInstances++;
			}

			Buffer::Description desc;
			desc.m_size = noofInstances * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
			desc.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
			desc.m_resourceFlags = D3D12_RESOURCE_FLAG_NONE;
			desc.m_heapType = D3D12_HEAP_TYPE_UPLOAD;

			Buffer* instanceDescriptionBuffer = new Buffer(desc, L"Instance Description Buffer");

			// Copy the instance data to the buffer
			UINT8* data;
			instanceDescriptionBuffer->GetResource()->Map(0, nullptr, (void**)&data);
			memcpy(data, instanceDescriptions, noofInstances * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
			instanceDescriptionBuffer->GetResource()->Unmap(0, nullptr);

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

			// Get the size requirements for the TLAS buffers
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
			ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
			ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			ASInputs.InstanceDescs = instanceDescriptionBuffer->GetResource()->GetGPUVirtualAddress();
			ASInputs.NumDescs = noofInstances;
			ASInputs.Flags = buildFlags;

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
			device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

			ASPreBuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
			ASPreBuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

			// Set TLAS size
			//dxr.tlasSize = ASPreBuildInfo.ResultDataMaxSizeInBytes;

			// Create TLAS scratch buffer
			Buffer::Description tlasDesc;
			tlasDesc.m_alignment = max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
			tlasDesc.m_elementSize = (uint)ASPreBuildInfo.ScratchDataSizeInBytes;
			tlasDesc.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
			tlasDesc.m_resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			tlasDesc.m_descriptorType = Buffer::DescriptorType::SRV | Buffer::DescriptorType::UAV;
			
			Buffer* tlasScratchBuffer = new Buffer(tlasDesc, L"TLAS Scratch Buffer");

			// Create the TLAS buffer
			tlasDesc.m_elementSize = (uint)ASPreBuildInfo.ResultDataMaxSizeInBytes;
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
			delete[] instanceDescriptions;

			scene->SetTLASBuffer(tlasBuffer);			 
		}
	}

	void CreateShaders()
	{
		Scene* scene = Graphics::Context.m_scene;
		Scene::DXR& dxr = scene->GetDXRData();
		ID3D12Device5* device = (ID3D12Device5*)Graphics::Context.m_device;

		//compile raygen shader
		{
			ShaderCompiler::Compile(L"Shaders\\RayGen.hlsl", L"", &dxr.m_raygenBlob);

			//create root signature
			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

			dxr.m_raygenRS.Reset(3, 0);
			dxr.m_raygenRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
			dxr.m_raygenRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4, D3D12_SHADER_VISIBILITY_ALL);
			dxr.m_raygenRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
			dxr.m_raygenRS.Finalise(device, L"Raygen shader RS", rootSignatureFlags);
		}

		//compile hit shader
		{
			ShaderCompiler::Compile(L"Shaders\\ClosestHit.hlsl", L"", &dxr.m_closestHitBlob);

			//create root signature
			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

			dxr.m_closestHitRS.Reset(3, 0);
			dxr.m_closestHitRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
			dxr.m_closestHitRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4, D3D12_SHADER_VISIBILITY_ALL);
			dxr.m_closestHitRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
			dxr.m_closestHitRS.Finalise(device, L"Closert Hit shader RS", rootSignatureFlags);
		}

		//compile miss shader
		{
			ShaderCompiler::Compile(L"Shaders\\Miss.hlsl", L"", &dxr.m_missBlob);
		}
	}

	void CreatePSO()
	{
		Scene* scene = Graphics::Context.m_scene;
		Scene::DXR& dxr = scene->GetDXRData();
		ID3D12Device5* device = (ID3D12Device5*)Graphics::Context.m_device;

		// Need 10 subobjects:
		// 1 for RGS program
		// 1 for Miss program
		// 1 for CHS program
		// 1 for Hit Group
		// 2 for RayGen Root Signature (root-signature and association)
		// 2 for Shader Config (config and association)
		// 1 for Global Root Signature
		// 1 for Pipeline Config	
		uint index = 0;
		vector<D3D12_STATE_SUBOBJECT> subObjects;
		subObjects.resize(10);

		// Add state subobject for the raygen shader
		{
			D3D12_EXPORT_DESC exportDesc = {};
			exportDesc.Name = L"RayGen_12";
			exportDesc.ExportToRename = L"RayGen";
			exportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

			D3D12_DXIL_LIBRARY_DESC	libDesc = {};
			libDesc.DXILLibrary.BytecodeLength = dxr.m_raygenBlob->GetBufferSize();
			libDesc.DXILLibrary.pShaderBytecode = dxr.m_raygenBlob->GetBufferPointer();
			libDesc.NumExports = 1;
			libDesc.pExports = &exportDesc;

			D3D12_STATE_SUBOBJECT subObject = {};
			subObject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
			subObject.pDesc = &libDesc;

			subObjects[index++] = subObject;
		}

		// Add state subobject for the Miss shader
		{
			D3D12_EXPORT_DESC exportDesc = {};
			exportDesc.Name = L"Miss_5";
			exportDesc.ExportToRename = L"Miss";
			exportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

			D3D12_DXIL_LIBRARY_DESC	libDesc = {};
			libDesc.DXILLibrary.BytecodeLength = dxr.m_missBlob->GetBufferSize();
			libDesc.DXILLibrary.pShaderBytecode = dxr.m_missBlob->GetBufferPointer();
			libDesc.NumExports = 1;
			libDesc.pExports = &exportDesc;

			D3D12_STATE_SUBOBJECT subObject = {};
			subObject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
			subObject.pDesc = &libDesc;

			subObjects[index++] = subObject;
		}

		// Add state subobject for the Closest Hit shader
		{
			D3D12_EXPORT_DESC exportDesc = {};
			exportDesc.Name = L"ClosestHit_76";
			exportDesc.ExportToRename = L"ClosestHit";
			exportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

			D3D12_DXIL_LIBRARY_DESC	libDesc = {};
			libDesc.DXILLibrary.BytecodeLength = dxr.m_closestHitBlob->GetBufferSize();
			libDesc.DXILLibrary.pShaderBytecode = dxr.m_closestHitBlob->GetBufferPointer();
			libDesc.NumExports = 1;
			libDesc.pExports = &exportDesc;

			D3D12_STATE_SUBOBJECT subObject = {};
			subObject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
			subObject.pDesc = &libDesc;

			subObjects[index++] = subObject;
		}

		// Add a state subobject for the hit group
		{
			D3D12_HIT_GROUP_DESC groupDesc = {};
			groupDesc.ClosestHitShaderImport = L"ClosestHit_76";
			groupDesc.HitGroupExport = L"HitGroup";

			D3D12_STATE_SUBOBJECT hitGroup = {};
			hitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
			hitGroup.pDesc = &groupDesc;

			subObjects[index++] = hitGroup;
		}

		// Add a state subobject for the shader payload configuration
		{
			D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
			shaderDesc.MaxPayloadSizeInBytes = sizeof(XMFLOAT4);	// only need float4 for color
			shaderDesc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;

			D3D12_STATE_SUBOBJECT shaderConfigObject = {};
			shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
			shaderConfigObject.pDesc = &shaderDesc;

			subObjects[index++] = shaderConfigObject;
		}

		// Create a list of the shader export names that use the payload
		const WCHAR* shaderExports[] = { L"RayGen_12", L"Miss_5", L"HitGroup" };

		// Add a state subobject for the association between shaders and the payload
		{
			D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
			shaderPayloadAssociation.NumExports = _countof(shaderExports);
			shaderPayloadAssociation.pExports = shaderExports;
			shaderPayloadAssociation.pSubobjectToAssociate = &subObjects[(index - 1)];

			D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
			shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
			shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;

			subObjects[index++] = shaderPayloadAssociationObject;
		}

		// Add a state subobject for the shared root signature 
		{
			D3D12_STATE_SUBOBJECT rayGenRootSigObject = {};
			rayGenRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
			rayGenRootSigObject.pDesc = dxr.m_raygenRS.GetSignature();

			subObjects[index++] = rayGenRootSigObject;

			// Create a list of the shader export names that use the root signature
			const WCHAR* rootSigExports[] = { L"RayGen_12", L"HitGroup", L"Miss_5" };

			// Add a state subobject for the association between the RayGen shader and the local root signature
			D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenShaderRootSigAssociation = {};
			rayGenShaderRootSigAssociation.NumExports = _countof(rootSigExports);
			rayGenShaderRootSigAssociation.pExports = rootSigExports;
			rayGenShaderRootSigAssociation.pSubobjectToAssociate = &subObjects[(index - 1)];

			D3D12_STATE_SUBOBJECT rayGenShaderRootSigAssociationObject = {};
			rayGenShaderRootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
			rayGenShaderRootSigAssociationObject.pDesc = &rayGenShaderRootSigAssociation;

			subObjects[index++] = rayGenShaderRootSigAssociationObject;

			D3D12_STATE_SUBOBJECT globalRootSig;
			globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
			globalRootSig.pDesc = dxr.m_missRS.GetSignature();

			subObjects[index++] = globalRootSig;
		}

		// Add a state subobject for the ray tracing pipeline config
		{
			D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
			pipelineConfig.MaxTraceRecursionDepth = 1;

			D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
			pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
			pipelineConfigObject.pDesc = &pipelineConfig;

			subObjects[index++] = pipelineConfigObject;
		}

		// Describe the Ray Tracing Pipeline State Object
		{
			D3D12_STATE_OBJECT_DESC pipelineDesc = {};
			pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
			pipelineDesc.NumSubobjects = static_cast<UINT>(subObjects.size());
			pipelineDesc.pSubobjects = subObjects.data();

			// Create the RT Pipeline State Object (RTPSO)
			ThrowIfFailed( device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&dxr.m_rtPSO)) );

			// Get the RTPSO properties
			ThrowIfFailed(dxr.m_rtPSO->QueryInterface(IID_PPV_ARGS(&dxr.m_rtpsoInfo)));
		}
	}

	void CreateHeap()
	{
		Scene* scene = Graphics::Context.m_scene;
		Scene::DXR& dxr = scene->GetDXRData();
		ID3D12Device5* device = (ID3D12Device5*)Graphics::Context.m_device;

		dxr.m_descriptorHeap = new GPUDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256);
		dxr.m_descriptorHeap->Reset();

		// Get the descriptor heap handle and increment size
		DescriptorHandle handle = dxr.m_descriptorHeap->GetHandleBlock(1); 
		dxr.m_descriptorHeap->AddToHandle(handle, Graphics::Context.m_shadowsCB->GetCBV());

		// Create the DXR output buffer UAV
		handle = dxr.m_descriptorHeap->GetHandleBlock(1); 
		dxr.m_descriptorHeap->AddToHandle(handle, Graphics::Context.m_shadowsRT->GetUAV());

		// Create the DXR Top Level Acceleration Structure SRV
		handle = dxr.m_descriptorHeap->GetHandleBlock(1);
		dxr.m_descriptorHeap->AddToHandle(handle, dxr.m_tlasBuffer->GetSRV());
	}

	void CreateShaderTable()
	{
		Scene* scene = Graphics::Context.m_scene;
		Scene::DXR& dxr = scene->GetDXRData();
		ID3D12Device5* device = (ID3D12Device5*)Graphics::Context.m_device;

		/*
		The Shader Table layout is as follows:
		Entry 0 - Ray Generation shader
		Entry 1 - Miss shader
		Entry 2 - Closest Hit shader
		All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
		The ray generation program requires the largest entry: sizeof(shader identifier) + 8 bytes for a descriptor table.
		The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT
		*/

		uint shaderIdSize = 32;
		uint sbtSize = 0;

		dxr.m_shaderTableEntrySize = shaderIdSize;
		dxr.m_shaderTableEntrySize += 8;					// CBV/SRV/UAV descriptor table
		dxr.m_shaderTableEntrySize = Align(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, dxr.m_shaderTableEntrySize);

		sbtSize = (dxr.m_shaderTableEntrySize * 3);		// 3 shader records in the table
		sbtSize = Align(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, sbtSize);

		// Create the shader table buffer
		Buffer::Description desc;
		desc.m_size = sbtSize;
		desc.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
		desc.m_resourceFlags = D3D12_RESOURCE_FLAG_NONE;
		desc.m_heapType = D3D12_HEAP_TYPE_UPLOAD;

		dxr.m_shaderTableBuffer = new Buffer(desc, L"Shader Table");

		// Map the buffer
		uint8_t* data;
		ThrowIfFailed( dxr.m_shaderTableBuffer->GetResource()->Map(0, nullptr, (void**)&data) );

		// Entry 0 - Ray Generation program and local root argument data (descriptor table with constant buffer and IB/VB pointers)
		memcpy(data, dxr.m_rtpsoInfo->GetShaderIdentifier(L"RayGen_12"), shaderIdSize);

		// Set the root arguments data. Point to start of descriptor heap
		*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(data + shaderIdSize) = dxr.m_descriptorHeap->GetHeapGPUStart();

		// Entry 1 - Miss program (no local root arguments to set)
		data += dxr.m_shaderTableEntrySize;
		memcpy(data, dxr.m_rtpsoInfo->GetShaderIdentifier(L"Miss_5"), shaderIdSize);

		// Entry 2 - Closest Hit program and local root argument data (descriptor table with constant buffer and IB/VB pointers)
		data += dxr.m_shaderTableEntrySize;
		memcpy(data, dxr.m_rtpsoInfo->GetShaderIdentifier(L"HitGroup"), shaderIdSize);

		// Set the root arg data. Point to start of descriptor heap
		*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(data + shaderIdSize) = dxr.m_descriptorHeap->GetHeapGPUStart();

		// Unmap
		dxr.m_shaderTableBuffer->GetResource()->Unmap(0, nullptr);
	}

	void InitResources()
	{
		CreateAccelerationStructure();
		CreateShaders();
		CreatePSO();
		CreateHeap();
		CreateShaderTable();
	}
}

#endif // DXR