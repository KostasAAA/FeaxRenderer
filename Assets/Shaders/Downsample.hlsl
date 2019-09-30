cbuffer cb : register(b0)
{
	float Width;
	float Height;
//	float2 pad;
}

Texture2D<float4> Input : register(t0);
RWTexture2D<float4> Output : register(u0);
SamplerState SamplerLinear : register(s0);

#define THREADX 8
#define THREADY 8

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x < Width && DTid.y < Height)
	{
		float2 uv = (DTid.xy + 0.5) / float2(Width, Height);

		Output[DTid.xy] = Input.SampleLevel(SamplerLinear, uv, 0);
	}
}