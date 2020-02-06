#include "stdafx.h"
#include "Graphics.h"
#include "DescriptorHeap.h"
#include "Depthbuffer.h"
#include "..\DXSample.h"


Depthbuffer::Depthbuffer(Description& desc)
: m_desc(desc)
, m_currentState(D3D12_RESOURCE_STATE_DEPTH_WRITE)
{
	ID3D12Device* device = Graphics::Context.m_device;
	DescriptorHeapManager* descriptorManager = Graphics::Context.m_descriptorManager;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = m_desc.m_format;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	DXGI_FORMAT format = DXGI_FORMAT_R32_TYPELESS;

	if (m_desc.m_format == DXGI_FORMAT_D16_UNORM)
	{
		format = DXGI_FORMAT_R16_TYPELESS;
	}

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(format, m_desc.m_width, m_desc.m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		m_currentState,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&m_depthStencil)
	));

	m_dsvHandle = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = m_desc.m_format;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHandle.GetCPUHandle());

	m_srvHandle = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	format = DXGI_FORMAT_R32_FLOAT;
	if (m_desc.m_format == DXGI_FORMAT_D16_UNORM)
	{
		format = DXGI_FORMAT_R16_UNORM;
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(m_depthStencil.Get(), &srvDesc, m_srvHandle.GetCPUHandle());
}

Depthbuffer::~Depthbuffer()
{
}

void Depthbuffer::TransitionTo(GraphicsContext& context, D3D12_RESOURCE_STATES stateAfter)
{
	if (stateAfter != m_currentState)
	{
		context.m_barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), m_currentState, stateAfter));
		m_currentState = stateAfter;
	}
}
