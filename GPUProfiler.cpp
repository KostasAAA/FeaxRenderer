
#if 0

#include "stdafx.h"


void ErrorPrintf(const char * message, ...) {}
void DebugPrintf(const char * message, ...) {}
float Time(){ return 1.0f; }


// GPUProfiler implementation



GPUProfiler::GPUProfiler ()
	: m_noofTimers(0)
{	
}

bool GPUProfiler::Init()
{
	ID3D12Device* device = Graphics::Context.m_device;
	uint64_t GpuFrequency;
	Graphics::g_CommandManager.GetCommandQueue()->GetTimestampFrequency(&GpuFrequency);
	m_gpuTickDelta = 1.0 / static_cast<double>(GpuFrequency);

	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.Type = D3D12_HEAP_TYPE_READBACK;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC BufferDesc;
	BufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	BufferDesc.Alignment = 0;
	BufferDesc.Width = sizeof(uint64_t) * sm_maxNoofTimers * Graphics::FrameCount;
	BufferDesc.Height = 1;
	BufferDesc.DepthOrArraySize = 1;
	BufferDesc.MipLevels = 1;
	BufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	BufferDesc.SampleDesc.Count = 1;
	BufferDesc.SampleDesc.Quality = 0;
	BufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	BufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ThrowIfFailed(device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readBackBuffer)));

	D3D12_QUERY_HEAP_DESC QueryHeapDesc;
	QueryHeapDesc.Count = sm_maxNoofTimers * Graphics::FrameCount;
	QueryHeapDesc.NodeMask = 1;
	QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	ThrowIfFailed(device->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(&queryHeap)));

	return true;
}

bool GPUProfiler::CreateTimestamp(WCHAR* name, UINT& timerID)
{

	return true;
}

void GPUProfiler::Shutdown ()
{
	for (UINT j = 0; j < NOOF_FRAMES; j++)
	{
		for (UINT i = 0; i < m_noofTimers; i++)
		{
			m_timeStamps[j][i].m_timeStamp->Release();
		}

		m_queryDisjoint[j]->Release();
	}

	m_noofTimers = 0;
}

void GPUProfiler::BeginFrame ()
{

}

void GPUProfiler::AddTimestamp(UINT timerID)
{
}

void GPUProfiler::EndFrame ()
{
}

GPUProfiler::TimeStamp& GPUProfiler::GetTimerData(UINT timerID)
{
	UINT frameToReturn = (Context::GetFrameNumber() - NOOF_FRAMES + 1) % NOOF_FRAMES;

	return m_timeStamps[frameToReturn][timerID];
}

void GPUProfiler::ResolveAllTimerData()
{
	UINT frameToCollect =(Context::GetFrameNumber() - NOOF_FRAMES + 1) % NOOF_FRAMES;

	ID3D11DeviceContext* d3dImmediateContext = Context::GetDeviceContext();

	// We shouldn't have to wait for data since we are reading counters from 2 frames ago, but just in case.
	while (d3dImmediateContext->GetData(m_queryDisjoint[frameToCollect], NULL, 0, 0) == S_FALSE);

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestampDisjoint;
	if (d3dImmediateContext->GetData(m_queryDisjoint[frameToCollect], &timestampDisjoint, sizeof(timestampDisjoint), 0) != S_OK)
	{
		DebugPrintf("Couldn't retrieve timestamp disjoint query data");
		return;
	}

	if (timestampDisjoint.Disjoint)
	{
		// Throw out this frame's data
		DebugPrintf("Timestamps disjoint");
		return;
	}

	UINT64 timestampPrev;
	if (d3dImmediateContext->GetData(m_timeStamps[frameToCollect][0].m_timeStamp, &timestampPrev, sizeof(UINT64), 0) != S_OK)
	{
		DebugPrintf("Couldn't retrieve timestamp query data for begin frame ");
		return;
	}

	UINT64 timestamp;
	if (d3dImmediateContext->GetData(m_timeStamps[frameToCollect][1].m_timeStamp, &timestamp, sizeof(UINT64), 0) != S_OK)
	{
		DebugPrintf("Couldn't retrieve timestamp query data");
		return;
	}

	const float N = 100.0f;

	//get the total frame time
	m_timeStamps[frameToCollect][1].m_duration *= (N - 1) / N;
	m_timeStamps[frameToCollect][1].m_duration += ((float(timestamp - timestampPrev) / float(timestampDisjoint.Frequency)) * 1000.0f) / N;

	for (UINT i = 2; i < m_noofTimers; i++)
	{

		if (d3dImmediateContext->GetData(m_timeStamps[frameToCollect][i].m_timeStamp, &timestamp, sizeof(UINT64), 0) != S_OK)
		{
			DebugPrintf("Couldn't retrieve timestamp query data");
			return;
		}

		m_timeStamps[frameToCollect][i].m_duration  *= (N - 1) / N;
		m_timeStamps[frameToCollect][i].m_duration += ((float(timestamp - timestampPrev) / float(timestampDisjoint.Frequency)) * 1000.0f) / N;

		timestampPrev = timestamp;
	}

}


#endif
