#pragma once

class Resource
{
public:

	Resource() {}
	virtual ~Resource() {};

	ID3D12Resource* GetResource(int index = 0) { return m_rendertarget[index]; }

	ID3D12DescriptorHeap* GetSRVHeap() { return m_srvHeap; }
	ID3D12DescriptorHeap* GetRTVHeap() { return m_rtvHeap; }
	ID3D12DescriptorHeap* GetUAVHeap() { return m_uavHeap; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(UINT index = 0)
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), index, m_rtvDescriptorSize);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetSRV(UINT index = 0)
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), index, m_srvDescriptorSize);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetUAV(UINT index = 0)
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_uavHeap->GetGPUDescriptorHandleForHeapStart(), index, m_uavDescriptorSize);
	}

	DXGI_FORMAT GetFormat() { return m_format; }

private:
	int m_width;
	int m_height;
	DXGI_FORMAT m_format;
	UINT m_nooRTs;
	UINT m_rtvDescriptorSize;
	UINT m_srvDescriptorSize;
	UINT m_uavDescriptorSize;

	std::vector<ID3D12Resource*> m_rendertarget;
	ID3D12DescriptorHeap* m_srvHeap;
	ID3D12DescriptorHeap* m_rtvHeap;
	ID3D12DescriptorHeap* m_uavHeap;

	void CreateResources(ID3D12Device* device, LPCWSTR name);
	void CreateDescriptors(ID3D12Device* device);

};


