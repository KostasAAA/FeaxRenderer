

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
	float	MipBias;
	float3  pad;
};

cbuffer perModelInstanceCB : register(b1)
{
	float4x4	World;
	float		AlbedoID;
	float		NormalID;
	float		Roughness;
	float		Metalness;
	float2		UVscale;
	float2		NormalScale;
};

Texture2D<float4> Textures[] : register(t0);
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

	float3 n = 0;

	if (NormalID >= 0)
	{
		n = Textures[NormalID].SampleBias(SamplerLinear, UVscale * input.uv, MipBias).rgb * 2 - 1;
	}

	float3 normal = normalize(input.normal.xyz +  float3(NormalScale * n.xy, 0));
	float4 albedo = Textures[AlbedoID].SampleBias(SamplerLinear, UVscale * input.uv, MipBias).rgba;

	clip(albedo.a - 0.5f);

	output.colour.rgb =  albedo;
	output.colour.a = Metalness;
	output.normal.xyz = normal;
	output.normal.w = Roughness;

    return output;
}
