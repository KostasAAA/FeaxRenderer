#include "stdafx.h"
#include "Graphics.h"
#include "DescriptorHeap.h"
#include "Texture.h"
#include "..\DXSample.h"
#define STB_IMAGE_IMPLEMENTATION
#include "..\stb_image.h"


// Compute the number of texture levels needed to reduce to 1x1.  This uses
// _BitScanReverse to find the highest set bit.  Each dimension reduces by
// half and truncates bits.  The dimension 256 (0x100) has 9 mip levels, same
// as the dimension 511 (0x1FF).
static inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
{
	uint32_t HighBit;
	_BitScanReverse((unsigned long*)&HighBit, Width | Height);
	return HighBit + 1;
}

Texture::Texture(int width, int height, int noofChannels, unsigned char *data)
: m_width(width)
, m_height(height)
, m_noofChannels(m_noofChannels)
, m_data(data)
, m_averageColour(0,0,0,1)
{

}

Texture::Texture(const char* filename, ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	stbi_uc *data = stbi_load(filename, &m_width, &m_height, &m_noofChannels, 0);

	m_data = new unsigned char[m_width * m_height * 4];
	
	int destIndex = 0;
	int srcIndex = 0;
	for (int y = 0; y < m_height; y++)
	{
		for (int x = 0; x < m_width; x++)
		{
			m_averageColour.x += pow(data[srcIndex] / 255.0f, 2.2f);
			m_averageColour.y += pow(data[srcIndex+1] / 255.0f, 2.2f);
			m_averageColour.z += pow(data[srcIndex+2] / 255.0f, 2.2f);

			m_data[destIndex++] = data[srcIndex++];
			m_data[destIndex++] = data[srcIndex++];
			m_data[destIndex++] = data[srcIndex++];

			if ( m_noofChannels == 4 )
				m_data[destIndex++] = data[srcIndex++];
			else
				m_data[destIndex++] = 255;

		}
	}

	m_averageColour.x /= m_width * m_height;
	m_averageColour.y /= m_width * m_height;
	m_averageColour.z /= m_width * m_height;

	m_noofChannels = 4;

	CreateResources(device, commandList);
}

void Texture::CreateResources(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	UINT16 noofMips = ComputeNumMips(m_width, m_height);

	// Describe and create a Texture2D.
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = noofMips;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Width = m_width;
	textureDesc.Height = m_height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_texture)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

	// Create the GPU upload buffer.
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_textureUploadHeap)));

	// Copy data to the intermediate upload heap and then schedule a copy 
	// from the upload heap to the Texture2D.

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = m_data;
	textureData.RowPitch = m_width * m_noofChannels;
	textureData.SlicePitch = textureData.RowPitch * m_height;

	UpdateSubresources(commandList, m_texture.Get(), m_textureUploadHeap.Get(), 0, 0, 1, &textureData);
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	// Describe and create a shader resource view (SRV) heap for the texture.
	DescriptorHeapManager* descriptorManager = Graphics::Context.m_descriptorManager;
	m_srvHandle = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = noofMips;
	device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHandle.GetCPUHandle());

	if (noofMips > 1)
	{
		GPUDescriptorHeap* gpuDescriptorHeap = descriptorManager->GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		ID3D12DescriptorHeap* ppHeaps[] = { gpuDescriptorHeap->GetHeap() };

		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		for (UINT16 i = 0; i < noofMips - 1; i++)
		{
			uint32_t srcWidth = m_width >> i;
			uint32_t srcHeight = m_height >> i;
			uint32_t dstWidth = srcWidth >> 1;
			uint32_t dstHeight = srcHeight >> 1;

			//create an srv for the previous texture mip
			DescriptorHandle srvHandle = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = i;

			device->CreateShaderResourceView(m_texture.Get(), &srvDesc, srvHandle.GetCPUHandle());

			// create a uav for the current texture mip.
			DescriptorHandle uavHandle = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = textureDesc.Format;
			uavDesc.Texture2D.MipSlice = i+1;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			device->CreateUnorderedAccessView(m_texture.Get(), nullptr, &uavDesc, uavHandle.GetCPUHandle());

			commandList->SetPipelineState(Graphics::Context.m_downsamplePSO.GetPipelineStateObject());
			commandList->SetComputeRootSignature(Graphics::Context.m_downsampleRS.GetSignature());

			DescriptorHandle srvHandleGPU = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(srvHandleGPU, srvHandle);

			DescriptorHandle uavHandleGPU = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(uavHandleGPU, uavHandle);

			float dims[] = { dstWidth , dstHeight };
			commandList->SetComputeRoot32BitConstants(0, 2, dims, 0);
			commandList->SetComputeRootDescriptorTable(1, srvHandleGPU.GetGPUHandle());
			commandList->SetComputeRootDescriptorTable(2, uavHandleGPU.GetGPUHandle());

			const uint32_t threadGroupSizeX = dstWidth / 8 + 1;
			const uint32_t threadGroupSizeY = dstHeight / 8 + 1;

			commandList->Dispatch(threadGroupSizeX, threadGroupSizeY, 1);

            commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_texture.Get()));

		}
	
	}
}

Texture::~Texture()
{
	delete[] m_data;
}
