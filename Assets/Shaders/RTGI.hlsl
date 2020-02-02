
#include "LightingCommon.h"

#define BACKFACE_CULLING 1
#include"raytracingCommon.h"

cbuffer cbGIRT : register(b0)
{
	float4x4	InvViewProjection;
	float4x4	ViewProjection;
	float4		RTSize;
	int			FrameIndex;
	float3		pad;
	float4		SampleVectors[100];
};

cbuffer LightsConstantBuffer : register(b1)
{
	uint	NoofDirectional;
	uint	NoofPoint;
	uint	NoofSpot;
	uint	LightsConstantBuffer_pad;
	DirectionalLightData DirectionalLight;
	PointLightData PointLights[100];
};

struct Material
{
	int			AlbedoID;
	int			NormalID;
	int			FurShells;
	float		Roughness;
	float		Metalness;
	float4		Albedo;
	float2	    UVScale;
    float2      NormalScale;
	float		Emissive;
};

Texture2D<float4>	rtgiHistoryBuffer: register(t0);
Texture2D<float>	depthBuffer : register(t1);
Texture2D<float4>	normalBuffer: register(t2);
Texture2D<float4>	albedoBuffer: register(t3);
BVHTreeBuffer		BVHTree : register(t4);
BVHNormalsBuffer	BVHNormals : register(t5);
BVHUVsBuffer		BVHUVs : register(t6);
StructuredBuffer<Material> Materials :  register(t7);
Texture2D<float4>	Diffuse[] : register(t8);

RWTexture2D<float4>			outputRT : register(u0);

SamplerState SamplerLinear : register(s0);

float rand01(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

#define THREADX 8
#define THREADY 8
#define THREADGROUPSIZE (THREADX*THREADY)

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	const uint2 screenPos = DTid.xy;

	bool showRays = (screenPos.x == RTSize.x / 4) && (screenPos.y == RTSize.y / 4);

//	float3 mainRT = mainBuffer[2 * screenPos].xyz;

	float depth = depthBuffer[4 * screenPos].r;

	if ( depth == 1)
	{
		outputRT[screenPos] = 0;// float4(mainRT, 1);
		return;
	}

	float3 albedo =   albedoBuffer[2 * screenPos].rgb;

	albedo = pow(albedo, 2.2);

	float4 normal = normalBuffer[4 * screenPos].xyzw;

	//get world position from depth
	float2 uv = (screenPos.xy + 0.5) * RTSize.zw;
	float4 clipPos = float4(2 * uv - 1, depth, 1);
	clipPos.y = -clipPos.y;

	float3 result = 0;

	float4 worldPos = mul(InvViewProjection, clipPos);
	worldPos.xyz /= worldPos.w;

	//offset to avoid self interesection
	//worldPos.xyz += 0.05 * normal.xyz;

//	float3 tangent = normalize( normal.z < normal.x ? (normal.y, -normal.x, 0) : (0, -normal.z, normal.y) );
	float3 tangent = 0;

	if (normal.z != 0)
		tangent = float3(1, 1, -(normal.x + normal.y) / normal.z);
	else if (normal.y != 0)
		tangent = float3(1, -(normal.x + normal.z) / normal.y, 1);
	else
		tangent = float3(-(normal.y + normal.z) / normal.x, 1, 1);
	
	tangent = normalize(tangent);

	//float3 tangent = normalize(cross(normal.xyz, float3(1, 0, 0)));
	float3 bitangent = normalize(cross(normal.xyz, tangent));

	int noofSamples =  SampleVectors[0].w / 4;

//	int index = (screenPos.y * RTSize.x + screenPos.x) % 3;

	float noofHits = 0;
	
	for (int i = 0; i < noofSamples; i++)
	{
//		int offset = rand01(uv) * (SampleVectors[0].w - 1);
//		int offset = FrameIndex * noofSamples + i;
		int offset = i;

		float3 rayDirection = SampleVectors[offset].x * tangent + SampleVectors[offset].y * bitangent + SampleVectors[offset].z * normal;

		rayDirection = normalize(rayDirection);

		HitData hitdata;

		if ( TraceRay(BVHTree, worldPos.xyz, rayDirection, false, hitdata) )
		{
			//interpolate normal
			float3 n0 = BVHNormals[hitdata.TriangleIndex * 3].xyz;
			float3 n1 = BVHNormals[hitdata.TriangleIndex * 3 + 1].xyz;
			float3 n2 = BVHNormals[hitdata.TriangleIndex * 3 + 2].xyz;

			float3 n = n0 * (1 - hitdata.BarycentricCoords.x - hitdata.BarycentricCoords.y) + n1 * hitdata.BarycentricCoords.x + n2 * hitdata.BarycentricCoords.y;
			n = normalize(n);

			//interpolate uvs
			//float2 uv0 = BVHUVs[hitdata.TriangleIndex * 3].xy;
			//float2 uv1 = BVHUVs[hitdata.TriangleIndex * 3 + 1].xy;
			//float2 uv2 = BVHUVs[hitdata.TriangleIndex * 3 + 2].xy;

			//float2 uvCoord = uv0 * (1 - hitdata.BarycentricCoords.x - hitdata.BarycentricCoords.y) + uv1 * hitdata.BarycentricCoords.x + uv2 * hitdata.BarycentricCoords.y;

			//get material data for hit point
			Material material = Materials[NonUniformResourceIndex(hitdata.MaterialID)];

			//sample albedo of hit point
			float3 albedoHitPoint = material.Albedo.rgb;

			//Diffuse[NonUniformResourceIndex(material.AlbedoID)].SampleLevel(SamplerLinear, uvCoord, 0).rgb;
			//albedo.rgb = pow(albedo.rgb, 2.2);

			float3 hitPoint = worldPos.xyz + hitdata.Distance * rayDirection;

			float bounchedNdotL = saturate(dot(normal.xyz, rayDirection));

			[loop]
			for (uint j = 0; j < NoofPoint; j++)
			{
				//bounced light calculations
				float dist = length(PointLights[j].Position.xyz - hitPoint.xyz);
				float3 lightDir = normalize(PointLights[j].Position.xyz - hitPoint.xyz);

				float falloff = 1 - saturate(dist / PointLights[j].Radius);

				float shadow = 1;

				float NdotL = saturate(dot(n, lightDir));

				float3 lightFromHitPoint =  (shadow * falloff * NdotL / PI)* albedoHitPoint.rgb * PointLights[j].Colour.xyz;
				
				albedo = 1;
				
				//bounced light reaching the surface
				result +=  (albedo * lightFromHitPoint) * bounchedNdotL;
			}

			noofHits++;
			//result += albedo;
//			 result += 1;
		}
	}

	result.rgb = lerp(rtgiHistoryBuffer[screenPos].rgb, result.rgb  , 0.01);

	outputRT[screenPos] = float4( result.rgb, 1);
}

