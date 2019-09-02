

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

struct PSInput
{
    float3 normal : NORMAL;
	float2 uv : TEXCOORD;
	float4 position : SV_POSITION;
};

cbuffer GPrepassCB : register(b0)
{
	float4x4 ViewProjection;
};

cbuffer perModelInstanceCB : register(b1)
{
	float4x4	World;
	float		AlbedoID;
	float		NormalID;
	float		Roughness;
	float		Metalness;
};

Texture2D<float4> Diffuse[] : register(t0);
SamplerState SamplerLinear : register(s0);

PSInput VSMain(VSInput input)
{
    PSInput result;

	result.position = mul(World, float4(input.position.xyz, 1));
	result.position = mul(ViewProjection, result.position);

	result.normal =  mul((float3x3)World, input.normal.xyz);
	result.uv = input.uv;

    return result;
}

struct PSOutput
{
	float4 colour : SV_Target0;
	float4 normal : SV_Target1;
};

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;

	float3 normal = normalize(input.normal.xyz);// *0.5 + 0.5;
	float3 albedo = Diffuse[AlbedoID].Sample(SamplerLinear, input.uv).rgb;

	output.colour.rgb = albedo;
	output.colour.a = Metalness;
	output.normal.xyz = normal;
	output.normal.w = Roughness;

    return output;
}
