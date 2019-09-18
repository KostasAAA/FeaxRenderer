
cbuffer cbPerPass : register(b0)
{
	float4x4	ViewProjPrevious;
	float4x4	InvViewProj;
	float4		RTSize;
	float4		AAConfig; 
	float2		JitterOffset;
	float2		pad;
}  
  
Texture2D<float4>			mainBuffer : register(t0);
Texture2D<float4>			historyBuffer : register(t1);
Texture2D<float>			depthBuffer : register(t2);

RWTexture2D<float4>			outputRT : register(u0);

SamplerState				SamplerLinear : register(s0);


// From "Temporal Reprojection Anti-Aliasing"
// https://github.com/playdeadgames/temporal
float3 ClipAABB(float3 aabbMin, float3 aabbMax, float3 prevSample, float3 avg)
{
#if 1
	// note: only clips towards aabb center (but fast!)
	float3 p_clip = 0.5 * (aabbMax + aabbMin);
	float3 e_clip = 0.5 * (aabbMax - aabbMin);

	float3 v_clip = prevSample - p_clip;
	float3 v_unit = v_clip.xyz / e_clip;
	float3 a_unit = abs(v_unit);
	float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

	if (ma_unit > 1.0)
		return p_clip + v_clip / ma_unit;
	else
		return prevSample;// point inside aabb
#else
	float3 r = prevSample - avg;
	float3 rmax = aabbMax - avg.xyz;
	float3 rmin = aabbMin - avg.xyz;

	const float eps = 0.000001f;

	if (r.x > rmax.x + eps)
		r *= (rmax.x / r.x);
	if (r.y > rmax.y + eps)
		r *= (rmax.y / r.y);
	if (r.z > rmax.z + eps)
		r *= (rmax.z / r.z);

	if (r.x < rmin.x - eps)
		r *= (rmin.x / r.x);
	if (r.y < rmin.y - eps)
		r *= (rmin.y / r.y);
	if (r.z < rmin.z - eps)
		r *= (rmin.z / r.z);

	return avg + r;
#endif
}

float Filter(in float x)
{
	int FilterMode = 0;

	if (FilterMode == 0)
	{
		return x;
	}
	//else if (FilterMode == 1)
	//{
	//	return FilterTriangle(x);
	//}
	//else if (FilterMode == 2)
	//{
	//	return FilterTriangle(x);
	//}
	//else //if ( FilterMode == 1 )
	//{
	//	return FilterBlackmanHarris(x);
	//}
}

#define THREADX 8
#define THREADY 8
#define THREADGROUPSIZE (THREADX*THREADY)

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	float2 screenPos = DTid.xy + 0.5;
	float2 uv = screenPos / RTSize.xy;

	float3 clrMin = 99999999.0f;
	float3 clrMax = -99999999.0f;

	float3 m1 = 0.0f;
	float3 m2 = 0.0f;
	float mWeight = 0.0f;

	float depth = depthBuffer[screenPos];

	float4 clipPos = float4(2 * uv - 1, depth, 1);
	clipPos.y = -clipPos.y;

	float4 worldPos = mul(InvViewProj, clipPos);
	worldPos.xyzw /= worldPos.w;

	float4 clipPosPrev = mul(ViewProjPrevious, worldPos);
	clipPosPrev.xy /= clipPosPrev.w;

	float2 uvPrev = clipPosPrev.xy * float2(0.5, -0.5) + float2(0.5, 0.5);

	float2 historyUV = uvPrev + JitterOffset;

	int SampleRadius = 1;

	for (int y = -SampleRadius; y <= SampleRadius; ++y)
	{
		for (int x = -SampleRadius; x <= SampleRadius; ++x)
		{
			float2 samplePos = screenPos + float2(x, y);

			float3 tap = mainBuffer[samplePos];

			clrMin = min(clrMin, tap);
			clrMax = max(clrMax, tap);

			m1 += tap;
			m2 += tap * tap;
			mWeight += 1.0f;
		}
	}

	float3 current = mainBuffer[int2(screenPos)].xyz;
	float3 history = historyBuffer.SampleLevel(SamplerLinear, historyUV, 0).rgb;

	int NeighborhoodClampMode = 3;//AAConfig.y;

	if (NeighborhoodClampMode == 1) // ClampModes_RGB_Clamp
	{
		history = clamp(history, clrMin, clrMax);
	}
	else if (NeighborhoodClampMode == 2 ) // ClampModes_RGB_Clip
	{
		history = ClipAABB(clrMin, clrMax, history, m1 / mWeight);
	}
	else if (NeighborhoodClampMode == 3 )//ClampModes_Variance_Clip
	{
		float VarianceClipGamma = 1;
		float3 mu = m1 / mWeight;
		float3 sigma = sqrt(abs(m2 / mWeight - mu * mu));
		float3 minc = mu - VarianceClipGamma * sigma;
		float3 maxc = mu + VarianceClipGamma * sigma;
		history = ClipAABB(minc, maxc, history, mu);
	}

	float3 result = lerp(history, current, AAConfig.x);

	outputRT[screenPos] = float4(result, 1);
}