cbuffer cbPerPass : register(b0)
{
	float4x4	ViewProjPrevious;
	float4x4	InvViewProj;
	float4		RTSize;
	float4		AAConfig;
	float2		JitterOffset;
	float2		pad;
}

Texture2D<float> DepthBuffer : register(t0);
RWTexture2D<float2> Output : register(u0);

#define THREADX 8
#define THREADY 8

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	float2 uv = (DTid.xy + 0.5) * RTSize.zw;

	float depth = DepthBuffer[DTid.xy].r;

	float4 clipPos = float4(2 * uv - 1, depth, 1);
	clipPos.y = -clipPos.y;

	float4 worldPos = mul(InvViewProj, clipPos);
	worldPos /= worldPos.w;

	float4 clipPosPrev = mul(ViewProjPrevious, worldPos);
	clipPosPrev.xy /= clipPosPrev.w;

	float2 uvPrev = clipPosPrev.xy * float2(0.5, -0.5) + float2(0.5, 0.5);

	float2 motionVector = (uv - uvPrev - JitterOffset) ;

	Output[DTid.xy].xy =  motionVector;
}