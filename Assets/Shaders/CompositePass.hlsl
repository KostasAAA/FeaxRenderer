#include "Common.hlsl"

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
	float4 colour : SV_Target0;
};

PSInput VSMain(VSInput input)
{
	PSInput result;

	result.position = float4(input.position.xyz, 1);
	result.uv = input.uv;

	return result;
}

Texture2D<float4> albedoBuffer : register(t0);
Texture2D<float4> lightDiffuseBuffer : register(t1);
Texture2D<float4> lightSpecularBuffer : register(t2);
Texture2D<float4> giBuffer : register(t3);

SamplerState SamplerLinear : register(s0);


PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;

	float4 albedo = albedoBuffer[input.position.xy].rgba;
	float3 diffuse = lightDiffuseBuffer[input.position.xy].rgb;
	float3 specular = lightSpecularBuffer[input.position.xy].rgb;

	float3 gi = giBuffer.SampleLevel(SamplerLinear, input.uv.xy, 0).rgb;

	float emissive = uint(albedo.w * 255) & 2;
	float metalness = uint(albedo.w * 255) & 1;

	output.colour.rgb = albedo.rgb;

	if (emissive == 0)
	{
		albedo.rgb = accurateSRGBToLinear(albedo.rgb);

		//albedo.rgb *= 1 - metalness;
		output.colour.rgb =  albedo* (diffuse + 0*gi) + specular;
	}

    return output;
}
