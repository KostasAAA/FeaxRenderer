//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

cbuffer SceneConstantBuffer : register(b0)
{
	float4x4 WorldViewProjection;
};

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

Texture2D Diffuse : register(t0);
SamplerState SamplerLinear : register(s0);

PSInput VSMain(VSInput input)
{
    PSInput result;

	result.position = mul(WorldViewProjection, float4(input.position.xyz, 1));
	result.normal = input.normal;
	result.uv = input.uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float3 lightDir = normalize(float3(1,1,-1));
	float3 normal = normalize(input.normal.xyz);
	float NdotL = saturate(dot(normal, lightDir));

	float3 albedo = Diffuse.Sample(SamplerLinear, input.uv);

	albedo = pow(albedo, 2.2);
	
	float3 result = 2* NdotL.xxx * albedo;

	result =  pow(result, 1 / 2.2);

    return float4(result,1);
}
