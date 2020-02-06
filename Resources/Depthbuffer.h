#pragma once

class DescriptorHandle;

class Depthbuffer
{
public:
	struct Description
	{
		int m_width;
		int m_height;
		DXGI_FORMAT m_format;
	};

	Depthbuffer(Description& desc);
	virtual ~Depthbuffer();

	ID3D12Resource* GetResource() { return m_depthStencil.Get(); }

	DescriptorHandle& GetDSV()
	{
		return m_dsvHandle;
	}

	DescriptorHandle& GetSRV()
	{
		return m_srvHandle;
	}

	DXGI_FORMAT GetFormat() { return m_desc.m_format; }

	void TransitionTo(GraphicsContext& context, D3D12_RESOURCE_STATES stateAfter);

private:
	Description m_desc;

	D3D12_RESOURCE_STATES m_currentState;
	ComPtr<ID3D12Resource> m_depthStencil;

	DescriptorHandle m_dsvHandle;
	DescriptorHandle m_srvHandle;
};


