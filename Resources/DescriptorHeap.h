#pragma once

class DescriptorHandle
{
public:
	DescriptorHandle()
	{
		m_CPUHandle.ptr = NULL;
		m_GPUHandle.ptr = NULL;
		m_HeapIndex = 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE& GetCPUHandle() { return m_CPUHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE& GetGPUHandle() { return m_GPUHandle; }
	UINT GetHeapIndex() { return m_HeapIndex; }

	void SetCPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) { m_CPUHandle = cpuHandle; }
	void SetGPUHandle(D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) { m_GPUHandle = gpuHandle; }
	void SetHeapIndex(UINT heapIndex) { m_HeapIndex = heapIndex; }

	bool IsValid() { return m_CPUHandle.ptr != NULL; }
	bool IsReferencedByShader() { return m_GPUHandle.ptr != NULL; }

private:
	D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE m_GPUHandle;
	UINT m_HeapIndex;
};

class DescriptorHeap
{
public:
	DescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool isReferencedByShader = false);
	virtual ~DescriptorHeap();

	ID3D12DescriptorHeap *GetHeap() { return m_DescriptorHeap.Get(); }
	D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() { return m_HeapType; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetHeapCPUStart() { return m_DescriptorHeapCPUStart; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetHeapGPUStart() { return m_DescriptorHeapGPUStart; }
	UINT GetMaxNoofDescriptors() { return m_MaxNoofDescriptors; }
	UINT GetDescriptorSize() { return m_DescriptorSize; }

	void AddToHandle(DescriptorHandle& destCPUHandle, DescriptorHandle& sourceCPUHandle)
	{
		Graphics::Context.m_device->CopyDescriptorsSimple(1, destCPUHandle.GetCPUHandle(), sourceCPUHandle.GetCPUHandle(), m_HeapType);
		destCPUHandle.GetCPUHandle().ptr += m_DescriptorSize;
	}

	void AddToHandle(DescriptorHandle& destCPUHandle, D3D12_CPU_DESCRIPTOR_HANDLE& sourceCPUHandle)
	{
		Graphics::Context.m_device->CopyDescriptorsSimple(1, destCPUHandle.GetCPUHandle(), sourceCPUHandle, m_HeapType);
		destCPUHandle.GetCPUHandle().ptr += m_DescriptorSize;
	}

protected:
	ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
	D3D12_CPU_DESCRIPTOR_HANDLE m_DescriptorHeapCPUStart;
	D3D12_GPU_DESCRIPTOR_HANDLE m_DescriptorHeapGPUStart;
	UINT m_MaxNoofDescriptors;
	UINT m_DescriptorSize;
	bool m_ReferencedByShader;
};


class CPUDescriptorHeap : public DescriptorHeap
{
public:
	CPUDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
	~CPUDescriptorHeap() final;

	DescriptorHandle GetNewHandle();
	void FreeHandle(DescriptorHandle handle);

private:
	std::vector<UINT> m_FreeDescriptors;
	UINT m_CurrentDescriptorIndex;
	UINT m_ActiveHandleCount;
};

class GPUDescriptorHeap : public DescriptorHeap
{
public:
	GPUDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
	~GPUDescriptorHeap() final {};

	void Reset();
	DescriptorHandle GetHandleBlock(UINT count);

private:
	UINT m_CurrentDescriptorIndex;
};


class DescriptorHeapManager
{
public:
	DescriptorHeapManager();
	~DescriptorHeapManager();

	DescriptorHandle CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType);
	DescriptorHandle CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count);

	GPUDescriptorHeap* GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
	{
		return m_GPUDescriptorHeaps[Graphics::Context.m_frameIndex][heapType];
	}

private:
	CPUDescriptorHeap* m_CPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	GPUDescriptorHeap* m_GPUDescriptorHeaps[Graphics::FrameCount][D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

};



/*




struct DescriptorHeapType
{
	enum Enum
	{
		SRV_CBV,
		RTV,
		UAV,
		DSV,
		Sampler,
		Noof
	};
};

class DescriptorAllocator
{
public:

	DescriptorAllocator();
	virtual ~DescriptorAllocator();

	ID3D12DescriptorHeap* GetHeap(DescriptorHeapType::Enum type) { return m_heaps[type]; }

	D3D12_CPU_DESCRIPTOR_HANDLE Allocate(DescriptorHeapType::Enum type, int count = 1);

private:
	const int m_maxNoofDescriptors = 256;

	ID3D12DescriptorHeap* m_heaps[DescriptorHeapType::Noof];
	UINT m_heapIndex[DescriptorHeapType::Noof];
	UINT m_descriptorSize[DescriptorHeapType::Noof];

	ID3D12DescriptorHeap*  CreateHeap(DescriptorHeapType::Enum type);
};


*/