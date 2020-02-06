
#include "LightingCommon.h"

#define BACKFACE_CULLING 1
#include"raytracingCommon.h"

cbuffer cbGIRT : register(b0)
{
	float4x4	InvViewProjection;
	float4		RTSize;
	float4		SampleVectors[20];
};

cbuffer LightsConstantBuffer : register(b1)
{
	float4	LightCounts;
	DirectionalLightData DirectionalLight;
	PointLightData PointLights[100];
};

struct Material
{
	float4		Albedo;
	int			AlbedoID;
	int			NormalID;
	float		Roughness;
	float		Metalness;
    float2	    UVScale;
    float2      NormalScale;
};

Texture2D<float4>	mainBuffer: register(t0);
Texture2D<float>	depthBuffer : register(t1);
Texture2D<float4>	normalBuffer: register(t2);
Texture2D<float4>	albedoBuffer: register(t3);
Buffer<float4>		BVHTree : register(t4);
Buffer<float4>		BVHNormals : register(t5);
Buffer<float2>		BVHUVs : register(t6);
StructuredBuffer<Material> Materials :  register(t7);
Texture2D<float4>	Diffuse[] : register(t8);

RWTexture2D<float4>			outputRT : register(u0);

SamplerState SamplerLinear : register(s0);

#define THREADX 8
#define THREADY 8
#define THREADGROUPSIZE (THREADX*THREADY)

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	const uint2 screenPos = DTid.xy;

	float3 mainRT = mainBuffer[screenPos].xyz;

	float depth = depthBuffer[screenPos].r;

	if ( depth == 1)
	{
		outputRT[screenPos] = float4(mainRT, 1);
		return;
	}

	float4 albedo = albedoBuffer[screenPos];
	float4 normal = normalBuffer[screenPos].xyzw;

	//get world position from depth
	float2 uv = (screenPos.xy + 0.5) * RTSize.zw;
	float4 clipPos = float4(2 * uv - 1, depth, 1);
	clipPos.y = -clipPos.y;

	float3result = 0;

	float4 worldPos = mul(InvViewProjection, clipPos);
	worldPos.xyz /= worldPos.w;

	//offset to avoid self interesection
	worldPos.xyz += 0.05 * normal.xyz;

	float3 tangent = normal.z < normal.x ? (normal.y, -normal.x, 0) : (0, -normal.z, normal.y);
	float3 bitangent = cross(normal, tangent);

	int noofSamples = SampleVectors[0].w;

	for (int i = 0; i < noofSamples; i++)
	{
		float3 rayDirection = SampleVectors[i].x * tangent + SampleVectors[i].y * bitangent + SampleVectors[i].z * normal;

		HitData hitdata;

		if (TraceRay(BVHTree, worldPos.xyz, rayDirection, false, hitdata))
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

			//sample texture of hit point
			float3 albedo = material.Albedo.rgb;
			//Diffuse[NonUniformResourceIndex(material.AlbedoID)].SampleLevel(SamplerLinear, uvCoord, 0).rgb;
			//albedo.rgb = pow(albedo.rgb, 2.2);

			[loop]
			for (int j = 0; j < int(LightCounts.y); j++)
			{
				float dist = length(PointLights[j].Position.xyz - worldPos.xyz);
				float3 lightDir = normalize(PointLights[j].Position.xyz - worldPos.xyz);

				float falloff = 1 - saturate(dist / PointLights[j].Radius);

				float shadow = 1;

				float NdotL = saturate(dot(n, lightDir));

				result += (shadow * falloff * NdotL) * albedo.rgb * PointLights[i].Colour.xyz;
			}
		}
	}

	outputRT[screenPos] = float4( result.rgb / noofSamples + mainRT.rgb, 1);
}

