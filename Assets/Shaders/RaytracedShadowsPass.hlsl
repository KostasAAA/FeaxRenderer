
#define BACKFACE_CULLING 0
#include"raytracingCommon.h"

cbuffer cbPerPass : register(b0)
{
	float4x4	projView;
	float4x4	invProjView;
	float4		rtSize;
	float4		lightDir; 
	float4		cameraPos;
}  
  
Texture2D<float>			depthBuffer : register(t0);
Texture2D<float4>			normalBuffer : register(t1);
BVHTreeBuffer				BVHTree : register(t2);
Texture2D<float>			shadowHistory : register(t3);
Texture2D<float4>			blueNoiseBuffer : register(t4);

RWTexture2D<float>			outputRT : register(u0);
 
#define THREADX 8
#define THREADY 8
#define THREADGROUPSIZE (THREADX*THREADY)


#define PI 3.14159265359

static float gActivateRotation = 1;

float rand01(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float2 randomOnDisk(float2 uv)
{
	float2 rand;
	rand.x = rand01(uv);
	rand.y = rand01(1-uv.yx);

	float rho = sqrt(rand.x);
	float phi = rand.y * 2 * PI + gActivateRotation * cameraPos.w;

	return rho * float2(cos(phi), sin(phi));
}

float2 randomOnDiskBlue(uint2 screenPos)
{
	float3 rand = blueNoiseBuffer[uint2(screenPos.x % 64, screenPos.y % 64)].xyz;

	float rho = sqrt(rand.x);
	float phi = rand.y * 2 * PI + gActivateRotation * cameraPos.w;

	return rho * float2(cos(phi), sin(phi));
}

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	float collision = 0;

	float depth = depthBuffer[DTid.xy].x;
	float3 normal = normalBuffer[DTid.xy].xyz; 

	float NdotL = dot(normal, normalize(lightDir.xyz));

	float2 uv = DTid.xy * rtSize.zw;

	gActivateRotation = 0;

	//do not raytrace for sky pixels and for surfaces that point away from the light
	if (depth < 1 && NdotL > 0)
	{

		if (uv.x >= 0.5)
			gActivateRotation = 0;

		//get world position from depth
		float4 clipPos = float4(2 * uv - 1, depth, 1);
		clipPos.y = -clipPos.y;

		float4 worldPos = mul(invProjView, clipPos);
		worldPos.xyz /= worldPos.w;

		//offset to avoid selfshadows
		worldPos.xyz += 0.1 * normal;

		float3 rayDir = lightDir.xyz;

#if 1
		float3 lightPerpX = normalize(cross(lightDir.xyz, float3(0, 1, 0)));
		float3 lightPerpY = normalize(cross(lightDir.xyz, lightPerpX));

		int w = 1;
		float scale = 0.02;

		for (int y = 0; y < w; y++)
		{
			for (int x = 0; x < w; x++)
			{
#if 0
				float ry = scale * (rand01((DTid.xy + float2(y, x))* rtSize.zw) - 0.5);
				float rx = scale * (rand01((DTid.yx + float2(x, y))* rtSize.zw) - 0.5);
#else
				float2 pointOnDisk = randomOnDiskBlue(DTid.xy + int2(x, y));

				float ry = 0.5 * scale * pointOnDisk.y;
				float rx = 0.5 * scale * pointOnDisk.x;
#endif
				rayDir = normalize(lightDir.xyz + rx * lightPerpX + ry * lightPerpY);

				collision += TraceRay(BVHTree, worldPos.xyz, rayDir);
			}
		}
		collision /= w*w;
#else
		collision = TraceRay(BVHTree, worldPos.xyz, rayDir);
#endif
	}

	float shadowFactor = 1 - float(collision);

//	float historyValue = shadowHistory[DTid.xy];

	outputRT[DTid.xy] = shadowFactor;
}