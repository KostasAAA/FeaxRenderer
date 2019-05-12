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

Texture2D<float4> colourBuffer : register(t0);

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;

	float3 albedo = albedoBuffer[input.position.xy].rgb;

	output.colour.rgb = pow ( albedo, 1/2.2);

	return output;
}
