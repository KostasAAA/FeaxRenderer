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

Texture2D<float4> colourBuffer : register(t0);

float4 PSMain(PSInput input) : SV_Target
{
	float4 output = 0;

	float3 albedo = colourBuffer[input.position.xy].rgb;

	output.rgb = pow ( abs(albedo), 1/2.2);

	return output;
}
