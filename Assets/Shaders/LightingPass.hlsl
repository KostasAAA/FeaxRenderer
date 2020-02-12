#include "LightingCommon.h"

#define BACKFACE_CULLING 0
#include"raytracingCommon.h"

//#define RAYTRACED_POINTLIGHT_SHADOWS

cbuffer LightingConstantBuffer : register(b0)
{
	float4x4	InvViewProjection;
	float4		CameraPos;
	float4		RTSize;
	float2		Jitter;
	float2		pad;
};

cbuffer LightsConstantBuffer : register(b1)
{
	uint	NoofDirectional;
	uint	NoofPoint;
	uint	NoofSpot;
	uint	LightsConstantBuffer_pad;
	float4	SkyLight;
	DirectionalLightData DirectionalLight;
	PointLightData PointLights[100];
};

#define MaxNoofShadowCasters 2
cbuffer ShadowAtlasPassCB : register(b2)
{
	float4x4 LightViewProjection[MaxNoofShadowCasters * 6];
};

struct VSInput
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
};

struct PSInput
{
	float2 uv : TEXCOORD;
	float4 position : SV_POSITION;
};

struct PSOutput
{
	float4 diffuse : SV_Target0;
	float4 specular : SV_Target1;
};

PSInput VSMain(VSInput input)
{
	PSInput result;

	result.position = float4(input.position.xyz, 1);
//	result.position.xy += Jitter.xy * RTSize.zw;
	result.uv = input.uv;

	return result;
}

Texture2D<float4> albedoBuffer : register(t0);
Texture2D<float4> normalBuffer : register(t1);
Texture2D<float> depthBuffer : register(t2);
Texture2D<float> shadowBuffer : register(t3);
Texture2D<float> shadowAtlas : register(t4);
BVHTreeBuffer	BVHTree : register(t5);

SamplerComparisonState SamplerShadows : register(s0);

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;

	float gaussian[5][5] = 
	{ 
		{ 1,  4,  7,  4, 1 },
		{ 4, 16, 26, 16, 4 },
		{ 7, 26, 41, 26, 7 },
		{ 4, 16, 26, 16, 4 },
		{ 1,  4,  7,  4, 1 },
	};

	float depth = depthBuffer[input.position.xy].x;

	if (depth < 1)
	{
		float4 normal = normalBuffer[input.position.xy];
		float4 albedo = albedoBuffer[input.position.xy];

		float roughness = normal.w;
		float metalness = albedo.w;
		float3 specularColour = lerp(0.04, albedo.rgb, metalness);

		float2 uv = input.position.xy * RTSize.zw;
		float4 clipPos = float4(2 * uv - 1, depth, 1);
		clipPos.y = -clipPos.y;

		float4 worldPos = mul(InvViewProjection, clipPos);
		worldPos.xyz /= worldPos.w;

		float3 viewDir = normalize(CameraPos.xyz - worldPos.xyz);

		int w = 2;
		float shadow = 0;

		for (int y = -w; y <= w; y++)
		{
			for (int x = -w; x <= w; x++)
			{
				float weight =  gaussian[x + w][y + w];
				//float sampleDepth = depthBuffer[input.position.xy + float2(x, y)].x;

				shadow += weight * shadowBuffer[input.position.xy + 2 * float2(x, y)].x;
			}
		}
		shadow /= 273.0;
		//shadow /= (2 * w + 1) * (2 * w + 1);

		shadow =   shadowBuffer[input.position.xy].x;

		//if (input.position.x >= 1280/2)
		 //	shadow = shadowBuffer[input.position.xy].x;
		
	//	if ( LightDirection.w > 0)
	//		shadow = shadowBuffer[input.position.xy].x;

		float3 lightDir = DirectionalLight.Direction.xyz;
		float3 lightColour = DirectionalLight.Colour.xyz;
		float lightIntensity = DirectionalLight.Intensity.x;
		float NdotL = saturate(dot(normal.xyz, lightDir));

		output.diffuse.rgb = (lightIntensity * shadow * DiffuseBRDF() * NdotL) * lightColour + 0.0 * lightIntensity;
		output.specular.rgb = (lightIntensity * shadow * NdotL) *  SpecularBRDF(normal.xyz, viewDir, lightDir, roughness, specularColour)  * lightColour;

		[loop]
		for (uint i = 0; i < NoofPoint; i++)
		{
			float dist = length(PointLights[i].Position.xyz - worldPos.xyz);
			lightDir = normalize(PointLights[i].Position.xyz - worldPos.xyz);
			NdotL = saturate(dot(normal.xyz, lightDir));

			if (dist > PointLights[i].Radius && NdotL == 0)
				continue;

#if defined(RAYTRACED_POINTLIGHT_SHADOWS)

			HitData hitdata;
			bool collision = TraceRay(BVHTree, worldPos.xyz + 0.01 * lightDir, lightDir, true, hitdata, dist);

			shadow = 1 - (collision && hitdata.Distance < dist);

#else
			int faceID = 0;

			float3 axes = abs(lightDir);
			float maxComponent = max(axes.x, max(axes.y, axes.z));

			if (maxComponent == axes.x) // x axis
			{
				faceID = lightDir.x <= 0 ? 0 : 2;
			}
			else if (maxComponent == axes.y) //y axis
			{
				faceID = lightDir.y <= 0 ? 4 : 5;
			}
			else //z axis
			{
				faceID = lightDir.z <= 0 ? 1 : 3;
			}

			float3 cols[] = 
			{
				float3(1,0,0),
				float3(0,1,0),
				float3(0,0,1),
				float3(1,0,1),
				float3(1,1,0),
				float3(0,1,1),
			};

			float4 lightSpacePos = mul(LightViewProjection[i * 6 + faceID], float4(worldPos.xyz, 1));
			lightSpacePos.xyz /= lightSpacePos.w;

			lightSpacePos.xy = lightSpacePos.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
			lightSpacePos.xy = PointLights[i].ShadowmapRange.xy + PointLights[i].ShadowmapRange.zw * float2(faceID, 0) + lightSpacePos.xy * PointLights[i].ShadowmapRange.zw;
				
			float shadowDepth = shadowAtlas.SampleCmpLevelZero(SamplerShadows, lightSpacePos.xy, lightSpacePos.z - 0.001).r;

			shadow = shadowDepth;// lightSpacePos.z < shadowDepth + 0.001;
#endif
			shadow = 1;
			float3 lightColour = PointLights[i].Colour.xyz;
			float lightIntensity = PointLights[i].Intensity;

			float3 worldToLight = PointLights[i].Position.xyz - worldPos.xyz;

			float falloff = SquareFalloffAttenuation(worldToLight, rcp(PointLights[i].Radius));  

			output.diffuse.rgb += (lightIntensity * shadow * falloff * NdotL * DiffuseBRDF() ) * lightColour;
			output.specular.rgb += (lightIntensity * shadow * falloff * NdotL) * SpecularBRDF(normal.xyz, viewDir, lightDir, roughness, specularColour) * lightColour;
		}
	}
	else
		output.diffuse.rgb = 1;

    return output;
}
