#if 0

// GPU performance-measurement subsystem

#pragma once

#define MAX_TIMESTAMPS 30
#define NOOF_FRAMES 3

class GPUProfiler
{
public:

	struct TimeStamp
	{
		WCHAR m_name[100];
		ID3D11Query * m_timeStamp;
		float m_duration;
	};

	GPUProfiler ();
	~GPUProfiler(){}

	bool Init ();
	void Shutdown ();
	bool CreateTimestamp(WCHAR* name, UINT& timerID);

	void BeginFrame();
	void AddTimestamp(UINT timerID);
	void EndFrame();

	void ResolveAllTimerData();
	TimeStamp& GetTimerData(UINT timerID);

	UINT GetNoofTimers() { return m_noofTimers; }
	 
protected:

	static const UINT sm_maxNoofTimers = 100;
	UINT m_noofTimers;
	double m_gpuTickDelta;

	ComPtr<ID3D12QueryHeap> queryHeap;
	ComPtr<ID3D12Resource> readBackBuffer;

	TimeStamp m_timeStamps[NOOF_FRAMES][MAX_TIMESTAMPS];		 
	float m_timerData[NOOF_FRAMES][MAX_TIMESTAMPS];

};




#endif