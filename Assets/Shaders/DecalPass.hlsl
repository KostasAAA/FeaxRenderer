

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD;
};

struct PSInput
{
    float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
	float4 position : SV_POSITION;
};

cbuffer GPrepassCB : register(b0)
{
	float4x4 ViewProjection;
	float4x4 InvViewProjection;
	float4	CameraPosition;
	float4	RTSize;
	float	MipBias;
	float3	pad;
};

cbuffer perModelInstanceCB : register(b1)
{
	float4x4	World;
	float4x4	ToObject;
	float		AlbedoID;
	float		NormalID;
	float		Roughness;
	float		Metalness;
	float4		Colour;
	float2		UVscale;
	float2		NormalScale;
	float		Emissive;
	float		FurShellsID;
	float2		perModelInstanceCB_pad;
};

Texture2D<float> depthBuffer : register(t0);
Texture2D<float4> Textures[] : register(t1);
SamplerState SamplerLinear : register(s0);

PSInput VSMain(VSInput input)
{
    PSInput result;

	result.normal = mul((float3x3)World, input.normal.xyz);
	result.tangent = mul((float3x3)World, input.tangent.xyz);
	result.position = mul(World, float4(input.position.xyz, 1));

	result.worldPos = result.position.xyz;

	result.position = mul(ViewProjection, result.position);

	result.uv = UVscale * input.uv;

    return result;
}

struct PSOutput
{
	float4 colour : SV_Target0;
	float4 normal : SV_Target1;
};

float3 GetWorldPosFromDepth(float2 screenPos, float depth)
{
	float2 uv = screenPos * RTSize.zw;
	float4 clipPos = float4(2 * uv - 1, depth, 1);
	clipPos.y = -clipPos.y;

	float4 worldPos = mul(InvViewProjection, clipPos);
	worldPos.xyz /= worldPos.w;

	return worldPos.xyz;
}

PSOutput PSMain(PSInput input)
{
	PSOutput output = (PSOutput)0;

	float3 n = 0;

	float4 albedo = Colour;
	
	float depth = depthBuffer[input.position.xy].x;

	float2 uv = input.position.xy * RTSize.zw;

	float3 worldPos = GetWorldPosFromDepth(input.position.xy, depth);

	float4 objectSpacePos = mul(ToObject, float4(worldPos.xyz, 1));

	clip(0.5 - abs(objectSpacePos.xyz));

	float3 ddxWp = ddx(worldPos);
	float3 ddyWp = ddy(worldPos);

	//Determine the normal
	float3 normal = normalize(cross(ddxWp, ddyWp));

	float3 absN = abs(normal);
	float maxAbs = max(max(absN.x, absN.y), absN.z);

	if(absN.x == maxAbs)
		uv = saturate( objectSpacePos.yz + 0.5 );
	else if (absN.y == maxAbs)
		uv = saturate(objectSpacePos.xz + 0.5);
	else 
		uv = saturate(objectSpacePos.xy + 0.5);
	
	if (AlbedoID >= 0)
	{
		albedo = Textures[NonUniformResourceIndex(AlbedoID)].SampleBias(SamplerLinear, uv, MipBias).rgba;
	}

	clip(albedo.a - 0.5f);

	if (NormalID >= 0)
	{
		float3 bitangent = normalize(ddyWp);
		float3 tangent = normalize(ddxWp);

		float3x3 tangent2World = float3x3(tangent, bitangent, normal);

		n = Textures[NonUniformResourceIndex(NormalID)].SampleBias(SamplerLinear, uv, MipBias).rgb * 2 - 1;

		normal = mul(n, tangent2World);
	}

	output.colour.rgb =  albedo.rgb;
	output.colour.a = 1;
	output.normal.xyz = normal;// normalize(input.normal.xyz + float3(NormalScale * n.yz, 0));
	output.normal.w = 1;

    return output;
}
