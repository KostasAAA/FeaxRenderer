#pragma once

class Rendertarget
{
public:

	Rendertarget(int width, int height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, XMFLOAT4 clearColour = { 0.0f,0.0f,0.0f,0.0f }, LPCWSTR name = L"Unknown RT");
	Rendertarget(IDXGISwapChain3* m_swapChain, LPCWSTR name = L"Unknown RT");
	Rendertarget() {}
	virtual ~Rendertarget();

	ID3D12Resource* GetResource() { return m_rendertarget; }

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

	DXGI_FORMAT GetFormat() { return m_format; }

	void TransitionTo(GraphicsContext& context, D3D12_RESOURCE_STATES stateAfter);

private:
	int m_width;
	int m_height;
	DXGI_FORMAT m_format;
	UINT m_nooRTs;
	D3D12_RESOURCE_FLAGS m_flags;

	XMFLOAT4 m_clearColour;

	D3D12_RESOURCE_STATES m_currentState;
	ID3D12Resource* m_rendertarget;

	DescriptorHandle m_rtvHandle;
	DescriptorHandle m_srvHandle;
	DescriptorHandle m_uavHandle;

	void CreateResources(LPCWSTR name);
	void CreateDescriptors();

};


