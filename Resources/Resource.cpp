#include "stdafx.h"
#include "Rendertarget.h"
#include "..\DXSample.h"


Rendertarget::Rendertarget(ID3D12Device* device, int width, int height, DXGI_FORMAT format, int noofRTs, LPCWSTR name)
: m_width(width)
, m_height(height)
, m_format(format)
, m_nooRTs(noofRTs)
{
	CreateResources(device, name);
	CreateDescriptors(device);
}

Rendertarget::Rendertarget(ID3D12Device* device, IDXGISwapChain3* m_swapChain, LPCWSTR name)
{
	ID3D12Resource* rendertarget;

	DXGI_SWAP_CHAIN_DESC desc;
	m_swapChain->GetDesc(&desc);

	m_nooRTs = desc.BufferCount;
	m_width = desc.BufferDesc.Width;
	m_height = desc.BufferDesc.Height;
	m_format = desc.BufferDesc.Format;

	for (UINT n = 0; n < m_nooRTs; n++)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&rendertarget)));

		rendertarget->SetName(name);

		m_rendertarget.push_back(rendertarget);
	}

	CreateDescriptors(device);
}

void Rendertarget::CreateResources(ID3D12Device* device, LPCWSTR name)
{
	DXGI_FORMAT format = m_format;

	// Describe and create a Texture2D.
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = format;
	textureDesc.Width = m_width;
	textureDesc.Height = m_height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;// | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = format;
	optimizedClearValue.Color[0] = 0.0f;
	optimizedClearValue.Color[1] = 0.0f;
	optimizedClearValue.Color[2] = 0.0f;
	optimizedClearValue.Color[3] = 0.0f;

	ID3D12Resource* rendertarget;

	for (UINT i = 0; i < m_nooRTs; i++)
	{
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&optimizedClearValue,
			IID_PPV_ARGS(&rendertarget)));

		rendertarget->SetName(name);

		m_rendertarget.push_back(rendertarget);
	}
}

void Rendertarget::CreateDescriptors(ID3D12Device* device)
{
	{
		// Describe and create a shader resource view (SRV) heap for the texture.
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = m_nooRTs;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

		// Describe and create a rt resource view heap for the texture.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = m_nooRTs;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {};
		uavHeapDesc.NumDescriptors = m_nooRTs;
		uavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		uavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&m_uavHeap)));
	}

	{
		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = m_format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		m_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = m_format;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = m_format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		m_uavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_uavHeap->GetCPUDescriptorHandleForHeapStart());

		for (UINT i = 0; i < m_nooRTs; i++)
		{

			device->CreateShaderResourceView(m_rendertarget[i], &srvDesc, srvHandle);
			srvHandle.Offset(1, m_srvDescriptorSize);

			device->CreateRenderTargetView(m_rendertarget[i], &rtvDesc, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);

		//	device->CreateUnorderedAccessView(m_rendertarget[i], nullptr, &uavDesc, uavHandle);
		//	uavHandle.Offset(1, m_uavDescriptorSize);
		}
	}
}

Rendertarget::~Rendertarget()
{
	for ( UINT i = 0 ; i < m_nooRTs; i++)
		m_rendertarget[i]->Release();

	m_srvHeap->Release();
	m_rtvHeap->Release();
	m_uavHeap->Release();

}

Rendertarget::TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter)
{

}
