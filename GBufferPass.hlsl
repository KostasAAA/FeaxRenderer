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

struct PSOutput
{
	float4 colour : SV_Target0;
	float4 normal : SV_Target1;
};

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;

	float3 normal = normalize(input.normal.xyz);
	float3 albedo = Diffuse.Sample(SamplerLinear, input.uv).rgb;

	output.colour.rgb = albedo;
	output.normal.xyz = normal;

    return output;
}
