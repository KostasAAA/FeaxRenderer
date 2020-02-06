

struct VSInput
{
	float3 position : POSITION;
	float3 colour : COLOUR;
};

struct PSInput
{
    float3 colour : COLOR;
	float4 position : SV_POSITION;
};

cbuffer DebugCB : register(b0)
{
	float4x4 ViewProjection;
	float	MipBias;
	float3  pad;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

	result.position = mul(ViewProjection, float4(input.position.xyz, 1));

	result.colour.rgb = input.colour.rgb;

    return result;
}

struct PSOutput
{
	float4 colour : SV_Target0;
};

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;

	output.colour.rgb = input.colour.rgb;
	output.colour.a = 1;

    return output;
}
