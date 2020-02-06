#include "LightingCommon.h"


cbuffer LightingConstantBuffer : register(b0)
{
	float4x4	InvViewProjection;
	float4		LightDirection;
	float4		CameraPos;
	float4		RTSize;
	float2		Jitter;
	float2		pad;
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

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;
	//int2 screenPos = input.uv * RTSize.xy;

	//output.diffuse.rgb = (screenPos.x % 2 == screenPos.y % 2) ? float3(1, 0, 0) : float3(0, 1, 0);

	//return output;

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

		shadow =  shadowBuffer[input.position.xy].x;

		//if (input.position.x >= 1280/2)
		 //	shadow = shadowBuffer[input.position.xy].x;
		
	//	if ( LightDirection.w > 0)
	//		shadow = shadowBuffer[input.position.xy].x;

		float3 lightDir = LightDirection.xyz;
		float lightIntensity = LightDirection.w;

		//shadow = 1;
		output.diffuse.rgb = lightIntensity * shadow * saturate(dot(normal.xyz, lightDir)) + 0.3;
		output.specular.rgb = lightIntensity * shadow * LightingGGX(normal.xyz, viewDir, lightDir, roughness, specularColour);
	}
	else
		output.diffuse.rgb = 1;

    return output;
}
