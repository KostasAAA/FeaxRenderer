#pragma once

#include "DescriptorHeap.h"

class Buffer
{
public:
	struct DescriptorType
	{
		enum Enum
		{
			SRV = 1 << 0,
			CBV = 1 << 1,
			UAV = 1 << 2,
			Structured = 1 << 3,
			Raw = 1 << 4,
		};
	};

	struct Description
	{
		uint m_noofElements;
		union
		{
			uint m_elementSize;
			uint m_size;
		};
		uint64 m_alignment;
		DXGI_FORMAT m_format;
		uint m_descriptorType;
		D3D12_RESOURCE_FLAGS m_resourceFlags;
		D3D12_RESOURCE_STATES m_state;
		D3D12_HEAP_TYPE m_heapType;

		Description() :
			m_noofElements(1)
			, m_elementSize(0)
			, m_alignment(0)
			, m_descriptorType(Buffer::DescriptorType::SRV)
			, m_format(DXGI_FORMAT_UNKNOWN)
			, m_resourceFlags(D3D12_RESOURCE_FLAG_NONE)
			, m_state(D3D12_RESOURCE_STATE_COMMON)
			, m_heapType(D3D12_HEAP_TYPE_DEFAULT)
		{}
	};

	Buffer(Description& description, LPCWSTR name = nullptr, unsigned char *data = nullptr);
	Buffer() {}
	virtual ~Buffer();

	ID3D12Resource* GetResource() { return m_buffer.Get(); }

	DescriptorHandle& GetSRV() { return m_srvHandle; }
	DescriptorHandle& GetCBV() { return m_cbvHandle; }

	uint8* Map()
	{
		if (m_cbvMappedData == nullptr && m_description.m_descriptorType & DescriptorType::CBV)
		{
			CD3DX12_RANGE readRange(0, 0);       
			ThrowIfFailed(m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvMappedData)));
		}

		return m_cbvMappedData;
	}

private:
	Description m_description;

	uint m_bufferSize;
	uint8* m_data;

	uint8* m_cbvMappedData;

	ComPtr<ID3D12Resource> m_buffer;
	ComPtr<ID3D12Resource> m_bufferUpload;

	DescriptorHandle m_srvHandle;
	DescriptorHandle m_cbvHandle;

	void CreateResources();
};


