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

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;

	float4 albedo = albedoBuffer[input.position.xy].rgba;
	float3 diffuse = lightDiffuseBuffer[input.position.xy].rgb;
	float3 specular = lightSpecularBuffer[input.position.xy].rgb;

	albedo.rgb = pow(albedo.rgb, 2.2);

	float metalness = albedo.w;
	albedo.rgb *= 1 - metalness;

	output.colour.rgb = albedo *diffuse + specular;

	//if (input.position.x >= 1280 / 2 - 1 && input.position.x <= 1280 / 2 + 1)
	//	output.colour.rgb = float3(1, 0, 0);

    return output;
}
