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
	result.uv = input.uv;

	return result;
}

cbuffer LightingConstantBuffer : register(b0)
{
	float4x4 InvWorldViewProjection;
	float4	LightDirection;
};

Texture2D<float4> albedoBuffer : register(t0);
Texture2D<float4> normalBuffer : register(t1);
Texture2D<float> depthBuffer : register(t2);
Texture2D<float> shadowBuffer : register(t3);

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;
	float depth = depthBuffer[input.position.xy].x;

	float3 normal = normalBuffer[input.position.xy].rgb * 2 - 1;
	float3 albedo = albedoBuffer[input.position.xy].rgb;
	float shadow = shadowBuffer[input.position.xy].r;

	float3 lightDir = LightDirection.xyz;

	if (depth < 1)
	{
		output.diffuse.rgb = shadow * saturate(dot(normal, lightDir)) + 0.3;
		output.specular.rgb = 0;
	}
	else
		output.diffuse.rgb = 1;

    return output;
}
