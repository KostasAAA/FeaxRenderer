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
		};
	};

	struct Description
	{
		uint m_noofElements;
		uint m_elementSize;
		uint64 m_alignment;
		DXGI_FORMAT m_format;
		D3D12_RESOURCE_FLAGS m_resourceFlags;
		D3D12_RESOURCE_STATES m_state;

		Description() :
			m_noofElements(0)
			, m_elementSize(0)
			, m_alignment(0)
			, m_format(DXGI_FORMAT_UNKNOWN)
			, m_resourceFlags(D3D12_RESOURCE_FLAG_NONE)
			, m_state(D3D12_RESOURCE_STATE_COMMON) {}
	};

	Buffer(Description& description, LPCWSTR name = nullptr, unsigned char *data = nullptr);
	Buffer() {}
	virtual ~Buffer();

	ID3D12Resource* GetResource() { return m_buffer; }

	DescriptorHandle& GetSRV() { return m_srvHandle; }

private:
	Description m_description;

	uint m_bufferSize;
	unsigned char* m_data;

	ID3D12Resource* m_buffer;
	ID3D12Resource* m_bufferUpload;

	DescriptorHandle m_srvHandle;

	void CreateResources();
};


