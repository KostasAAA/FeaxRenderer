
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
Texture2D<float2>			velocityBuffer : register(t3);

RWTexture2D<float4>			outputRT : register(u0);

SamplerState				SamplerLinear : register(s0);

#define PI 3.14159265359

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


static float FilterTriangle(in float x)
{
	return saturate(1.0f - x);
}

float FilterBlackmanHarris(in float x)
{
	x = 1.0f - x;

	const float a0 = 0.35875f;
	const float a1 = 0.48829f;
	const float a2 = 0.14128f;
	const float a3 = 0.01168f;
	return saturate(a0 - a1 * cos(PI * x) + a2 * cos(2 * PI * x) - a3 * cos(3 * PI * x));
}

float Filter(in float x, in int filterMode)
{
	if (filterMode == 1)
	{
		return FilterTriangle(x);
	}
	else if (filterMode == 2)
	{
		return FilterBlackmanHarris(x);
	}

	return x;
}

float2 LoadVelocity(int2 screenPos)
{
	float2 velocity = 0;

	int DilationMode = AAConfig.w;

	if (DilationMode == 0)
	{
		velocity = velocityBuffer[screenPos].xy;
	}
	else if (DilationMode == 1) // nearest depth
	{
		float closestDepth = 10.0f;
		for (int y = -1; y <= 1; y++)
		{
			for (int x = -1; x <= 1; x++)
			{
				float2 neighborVelocity = velocityBuffer[screenPos + int2(x, y)].xy;
				float neighborDepth = depthBuffer[screenPos + int2(x, y)].x;

				if (neighborDepth < closestDepth)
				{
					velocity = neighborVelocity;
					closestDepth = neighborDepth;
				}
			}
		}
	}
	else if (DilationMode == 2) // greatest velocity
	{
		float greatestVelocity = -1.0f;
		for (int y = -1; y <= 1; y++)
		{
			for (int x = -1; x <= 1; x++)
			{
				float2 neighborVelocity = velocityBuffer[screenPos + int2(x, y)].xy;
				float neighborVelocityMag = dot(neighborVelocity, neighborVelocity);
				if (dot(neighborVelocity, neighborVelocity) > greatestVelocity)
				{
					velocity = neighborVelocity;
					greatestVelocity = neighborVelocityMag;
				}
			}
		}
	}

	return velocity;
}

#define THREADX 8
#define THREADY 8

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

	//float2 historyUV1 = uvPrev + JitterOffset;

	//load velocity
	float2 historyUV = uv - LoadVelocity(screenPos);

	//outputRT[screenPos] = float4(10000*abs(historyUV- historyUV1), 0, 1);
	//return;

	//find area min max
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
	float3 history;
	
	int ReprojectionMode = AAConfig.z;

	if (ReprojectionMode == 0)
	{
		history = historyBuffer[screenPos].rgb;
	}
	else if (ReprojectionMode == 1)
	{
		history = historyBuffer.SampleLevel(SamplerLinear, historyUV, 0).rgb;
	}
	else
	{
		float3 sum = 0.0f;
		float totalWeight = 0.0f;
		int ReprojectionFilter = 1;

		float2 reprojectedPos = historyUV * RTSize.xy;
		for (int y = -1; y <= 2; y++)
		{
			for (int x = -1; x <= 2; x++)
			{
				float2 samplePos = floor(reprojectedPos + float2(x, y)) + 0.5f;
				float3 reprojectedSample = historyBuffer[int2(samplePos)].xyz;

				float2 sampleDist = abs(samplePos - reprojectedPos);
				float filterWeight = Filter(sampleDist.x, ReprojectionFilter) * Filter(sampleDist.y, ReprojectionFilter);

				//float sampleLum = Luminance(reprojectedSample);

				//if (UseExposureFiltering)
				//	sampleLum *= exp2(ManualExposure - ExposureScale + ExposureFilterOffset);

				//if (InverseLuminanceFiltering)
				//	filterWeight *= 1.0f / (1.0f + sampleLum);

				sum += reprojectedSample * filterWeight;
				totalWeight += filterWeight;
			}
		}

		history = max(sum / totalWeight, 0.0f);
	}

	int NeighborhoodClampMode = AAConfig.y;

	if (NeighborhoodClampMode == 1) // RGB Clamp
	{
		history = clamp(history, clrMin, clrMax);
	}
	else if (NeighborhoodClampMode == 2 ) // RGB Clip
	{
		history = ClipAABB(clrMin, clrMax, history, m1 / mWeight);
	}
	else if (NeighborhoodClampMode == 3 )// Variance Clip
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