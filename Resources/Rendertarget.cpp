#include "stdafx.h"
#include "Graphics.h"
#include "DescriptorHeap.h"
#include "Rendertarget.h"
#include "..\DXSample.h"


Rendertarget::Rendertarget(Description& desc)
: m_desc(desc)
{
	CreateResources(L"");
	CreateDescriptors();
}

void Rendertarget::CreateResources(LPCWSTR name)
{
	if (m_desc.m_buffer)
	{
		m_rendertarget = m_desc.m_buffer;
		return;
	}

	ID3D12Device* device = Graphics::Context.m_device;

	DXGI_FORMAT format = m_desc.m_format;

	// Describe and create a Texture2D.
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = format;
	textureDesc.Width = m_desc.m_width;
	textureDesc.Height = m_desc.m_height;
	textureDesc.Flags = m_desc.m_flags;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = format;
	optimizedClearValue.Color[0] = m_desc.m_clearColour.x;
	optimizedClearValue.Color[1] = m_desc.m_clearColour.y;
	optimizedClearValue.Color[2] = m_desc.m_clearColour.z;
	optimizedClearValue.Color[3] = m_desc.m_clearColour.w;

	m_currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		m_currentState,
		&optimizedClearValue,
		IID_PPV_ARGS(&m_rendertarget)));

	m_rendertarget->SetName(name);
}

void Rendertarget::CreateDescriptors()
{
	ID3D12Device* device = Graphics::Context.m_device;
	DescriptorHeapManager* descriptorManager = Graphics::Context.m_descriptorManager;

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_desc.m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = m_desc.m_format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = m_desc.m_format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	m_rtvHandle = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(m_rendertarget.Get(), &rtvDesc, m_rtvHandle.GetCPUHandle());

	m_srvHandle = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	device->CreateShaderResourceView(m_rendertarget.Get(), &srvDesc, m_srvHandle.GetCPUHandle());

	if (m_desc.m_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		m_uavHandle = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CreateUnorderedAccessView(m_rendertarget.Get(), nullptr, &uavDesc, m_uavHandle.GetCPUHandle());
	}
}

Rendertarget::~Rendertarget()
{
}

void Rendertarget::TransitionTo(GraphicsContext& context, D3D12_RESOURCE_STATES stateAfter)
{
	if (stateAfter != m_currentState)
	{
		context.m_barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), m_currentState, stateAfter));
		m_currentState = stateAfter;
	}
}
