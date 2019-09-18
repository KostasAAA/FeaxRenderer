
cbuffer cbPerPass : register(b0)
{
	float4x4	projView;
	float4x4	invProjView;
	float4		rtSize;
	float4		aaConfig; 
}  
  
Texture2D<float4>			mainBuffer : register(t0);
Texture2D<float4>			historyBuffer : register(t1);

RWTexture2D<float4>			outputRT : register(u0);
 
#define THREADX 8
#define THREADY 8
#define THREADGROUPSIZE (THREADX*THREADY)

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
//	float2 uv = DTid.xy * rtSize.zw;

	float3 current = mainBuffer[DTid.xy].xyz;
	float3 history = historyBuffer[DTid.xy].xyz;

	float3 result = lerp(history, current, aaConfig.x);

	outputRT[DTid.xy] = float4(result, 1);
}