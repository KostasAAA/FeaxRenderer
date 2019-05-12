
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
Buffer<float4>				BVHTree : register(t2);
Texture2D<float>			shadowHistory : register(t3);

RWTexture2D<float>			outputRT : register(u0);
 
//Texture2D					diffuseMaps[] : register(t5, space1);
//SamplerState				samplerPoint : register(s6);

#define THREADX 8
#define THREADY 8
#define THREADGROUPSIZE (THREADX*THREADY)

bool TraceRay(float3 worldPos, float3 rayDir)
{
	float t = 0;
	float2 bCoord = 0;

	int dataOffset = 0;
	bool done = false;

	bool collision = false;

	int offsetToNextNode = 1;

	float3 rayDirInv = rcp(rayDir);

	[loop]
	while (offsetToNextNode != 0)
	{
		float4 element0 = BVHTree[dataOffset++].xyzw;

		offsetToNextNode = int(element0.w);

		collision = false;

		if (offsetToNextNode < 0)
		{
			float4 element1 = BVHTree[dataOffset++].xyzw;

			//try collision against this node's bounding box	
			float3 bboxMin = element0.xyz;
			float3 bboxMax = element1.xyz;

			//intermediate node check for intersection with bounding box
			collision = RayIntersectsBox(worldPos.xyz, rayDirInv, bboxMin.xyz, bboxMax.xyz);

			//if there is collision, go to the next node (left) or else skip over the whole branch
			if (!collision)
				dataOffset -= offsetToNextNode;
		}
		else if (offsetToNextNode > 0)
		{
			float4 vertex0 = element0;
			float4 vertex1MinusVertex0 = BVHTree[dataOffset++].xyzw;
			float4 vertex2MinusVertex0 = BVHTree[dataOffset++].xyzw;

			//check for intersection with triangle
			collision = RayTriangleIntersect(worldPos.xyz, rayDir, vertex0.xyz, vertex1MinusVertex0.xyz, vertex2MinusVertex0.xyz, t, bCoord);

			if (collision)
				return true;
		}
	};

	return false;
}

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
	float phi = rand.y * 2 * PI + gActivateRotation * cameraPos.w /(2* PI);

	return rho * float2(cos(phi), sin(phi));
}

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	float collision = 0;
	 
	float depth = depthBuffer[DTid.xy].x;
	float3 normal = normalBuffer[DTid.xy].xyz * 2 - 1;

	float NdotL = dot(normal, normalize(lightDir.xyz));

	float2 uv = DTid.xy * rtSize.zw;

	gActivateRotation = 1;

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
		worldPos.xyz += 0.01 * normal;

		float3 rayDir = lightDir.xyz;
		float3 lightPerpX = normalize(cross(lightDir.xyz, float3(0,1,0)));
		float3 lightPerpY = normalize(cross(lightDir.xyz, lightPerpX));

		int w = 2;
		float scale = 0.04;
		//scale = 1;

		for (int y = 0; y < w; y++)
		{
			for (int x = 0; x < w; x++)
			{
#if 0
				float ry = scale * (rand01((DTid.xy + float2(y, x))* rtSize.zw)-0.5);
				float rx = scale * (rand01((DTid.yx + float2(x, y))* rtSize.zw)-0.5);
#else
				float2 pointOnDisk = randomOnDisk( uv + float2(x, y)* rtSize.zw );

				float ry = 0.5*scale * pointOnDisk.y;
				float rx = 0.5*scale * pointOnDisk.x;
#endif
				rayDir = normalize(lightDir.xyz + rx * lightPerpX + ry * lightPerpY);

				collision += TraceRay(worldPos.xyz, rayDir);
			}
		}
		collision /= w*w;
	}
	float shadowFactor = 1 - float(collision);

	float historyValue = shadowHistory[DTid.xy];
	if (cameraPos.w < 10)
		historyValue = 1;

	if ( uv.x < 0.5)
		outputRT[DTid.xy] =  lerp(shadowFactor, historyValue, 0.9);
	else
		outputRT[DTid.xy] = shadowFactor;

	//historyRT[DTid.xy] = 
}