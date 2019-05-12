#pragma once

#include "DescriptorHeap.h"

class Buffer
{
public:

	Buffer(uint noofElements, uint elementSize, DXGI_FORMAT format, unsigned char *data = nullptr, LPCWSTR name = nullptr);
	Buffer() {}
	virtual ~Buffer();

	ID3D12Resource* GetResource() { return m_buffer; }

	DescriptorHandle& GetSRV() { return m_srvHandle; }

private:
	uint m_noofElements;
	uint m_elementSize;
	uint m_bufferSize;
	DXGI_FORMAT m_format;
	D3D12_RESOURCE_FLAGS m_resourceFlags;

	unsigned char* m_data;

	ID3D12Resource* m_buffer;
	ID3D12Resource* m_bufferUpload;

	DescriptorHandle m_srvHandle;

	void CreateResources();
};


