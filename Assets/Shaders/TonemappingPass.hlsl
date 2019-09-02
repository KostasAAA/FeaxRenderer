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

PSInput VSMain(VSInput input)
{
	PSInput result;

	result.position = float4(input.position.xyz, 1);
	result.uv = input.uv;

	return result;
}

cbuffer LightingConstantBuffer : register(b0)
{
	unsigned int LightingDebug;
}

Texture2D<float4> colourBuffer : register(t0);
Texture2D<float4> ssrBuffer : register(t1);

float4 PSMain(PSInput input) : SV_Target
{
	float4 output = 0;

	float3 albedo = colourBuffer[input.position.xy].rgb;
	float3 ssr = ssrBuffer[input.position.xy].rgb;

	output.rgb = pow ( abs(LightingDebug.x * albedo + ssr), 1/2.2);

	return output;
}
