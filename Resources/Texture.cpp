#include "stdafx.h"
#include "Graphics.h"
#include "DescriptorHeap.h"
#include "Texture.h"
#include "..\DXSample.h"
#define STB_IMAGE_IMPLEMENTATION
#include "..\stb_image.h"

Texture::Texture(int width, int height, int noofChannels, unsigned char *data)
: m_width(width)
, m_height(height)
, m_noofChannels(m_noofChannels)
, m_data(data)
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
			m_data[destIndex++] = data[srcIndex++];
			m_data[destIndex++] = data[srcIndex++];
			m_data[destIndex++] = data[srcIndex++];

			if ( m_noofChannels == 4 )
				m_data[destIndex++] = data[srcIndex++];
			else
				m_data[destIndex++] = 255;

		}
	}

	m_noofChannels = 4;

	CreateResources(device, commandList);
}

void Texture::CreateResources(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	// Describe and create a Texture2D.
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Width = m_width;
	textureDesc.Height = m_height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
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

	//D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	//srvHeapDesc.NumDescriptors = 1;
	//srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	//srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	//ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHandle.GetCPUHandle());
}

Texture::~Texture()
{
	delete[] m_data;
}
