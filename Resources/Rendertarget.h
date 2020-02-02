#pragma once

class DescriptorHandle;

class Rendertarget
{
public:
	struct Description
	{
		int m_width;
		int m_height;
		DXGI_FORMAT m_format;
		D3D12_RESOURCE_FLAGS m_flags;
		XMFLOAT4 m_clearColour;
		ComPtr<ID3D12Resource> m_buffer;
	};

	Rendertarget(Description& desc);
	Rendertarget() {}
	virtual ~Rendertarget();

	ID3D12Resource* GetResource() { return m_rendertarget.Get(); }

	DescriptorHandle& GetRTV()
	{
		return m_rtvHandle;
	}

	DescriptorHandle& GetSRV()
	{
		return m_srvHandle;
	}

	DescriptorHandle& GetUAV()
	{
		return m_uavHandle;
	}

	DXGI_FORMAT GetFormat() { return m_desc.m_format; }

	void TransitionTo(GraphicsContext& context, D3D12_RESOURCE_STATES stateAfter);

	int GetWidth() { return m_desc.m_width; }
	int GetHeight() { return m_desc.m_height; }
private:
	Description m_desc;

	D3D12_RESOURCE_STATES m_currentState;
	ComPtr<ID3D12Resource> m_rendertarget;

	DescriptorHandle m_rtvHandle;
	DescriptorHandle m_srvHandle;
	DescriptorHandle m_uavHandle;

	void CreateResources(LPCWSTR name);
	void CreateDescriptors();

};


