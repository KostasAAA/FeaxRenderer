

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
	float4x4 InverseViewProjection;
	float4  CameraPosition;
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

Texture2D<float4> Textures[] : register(t0);
SamplerState SamplerLinear : register(s0);

static const float FurScale = 0.01;

PSInput VSMain(VSInput input)
{
    PSInput result;

	result.normal = mul((float3x3)World, input.normal.xyz);
	result.tangent = mul((float3x3)World, input.tangent.xyz);

	result.position = mul(World, float4(input.position.xyz, 1));

	if (FurShellsID > 0)
	{
		result.position.xyz += FurScale * result.normal.xyz;
	}

	result.worldPos = result.position.xyz;

	result.position = mul(ViewProjection, result.position);

	result.uv =3* UVscale * input.uv;

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

	float4 albedo = Colour;
		
	float fur = 1;

	float currHeight = 1.0f;
	float2 finalTexOffset = input.uv;

	bool foundHit = false;
	if (FurShellsID >= 0)
	{
		const int SampleCount = 64;
		float gHeightScale = 3.5;

		float3 viewDir = normalize(input.worldPos - CameraPosition.xyz);

		float3 gravity = float3(0, -1, 0);

		float3 N = normalize(input.normal.xyz);
		float3 T = normalize(input.tangent.xyz -dot(input.tangent.xyz, N) * N);
		float3 B = cross(N, T);

		float3x3 toTangent = float3x3(T, B, N);

		float3 viewDirTS = mul(toTangent, viewDir );
		float3 gravityTS = mul(toTangent, gravity);

		float2 maxParallaxOffset = -viewDirTS.xy * gHeightScale / viewDirTS.z;

		//maxParallaxOffset -= 0.01*gravityTS.xy / gravityTS.z;

		float zStep = 1.0f / (float)SampleCount;
		float2 texStep = maxParallaxOffset * zStep;

		float2 dx = ddx(input.uv);
		float2 dy = ddy(input.uv);
		int sampleIndex = 0;
		float2 currTexOffset = 0;
		float2 prevTexOffset = 0;
		finalTexOffset = 0;
		float currRayZ = 1.0f -zStep;
		float prevRayZ = 1.0f;
		currHeight = 0.0f;
		float prevHeight = 0.0f;

		for (int i = 0; i < SampleCount; i++)
		{
			currHeight = Textures[FurShellsID].SampleGrad(SamplerLinear, (input.uv + currTexOffset), dx, dy).x;

			 currHeight *= saturate(sin( 0.5*input.uv.x));

			// Did we cross the height profile?
			if ( currRayZ < currHeight )
			{
				// Do ray/line segment intersection and compute final tex offset.
				float t = (prevHeight - prevRayZ) / (prevHeight - currHeight + currRayZ - prevRayZ);
				finalTexOffset = prevTexOffset + t * texStep;

				// Exit loop.
				foundHit = true;
				break;
			}
			else
			{
				prevTexOffset = currTexOffset;
				prevRayZ = currRayZ;
				prevHeight = currHeight;
				currTexOffset += texStep;
				// Negative because we are going "deeper" into the surface.
				currRayZ -= zStep;
			}
		}
	}

	if (AlbedoID >= 0)
	{
		albedo = Textures[AlbedoID].SampleBias(SamplerLinear, finalTexOffset, MipBias).rgba;
	}

	albedo *= currHeight;

	if (FurShellsID >= 0 && !foundHit )
	{
		clip(-1);
	}

	clip(albedo.a - 0.5f);

	if (NormalID >= 0)
	{
		n = Textures[NormalID].SampleBias(SamplerLinear, input.uv, MipBias).rgb * 2 - 1;
	}

	uint mask = Metalness > 0 ? 1 : 0;

	if (Emissive > 0)
		mask = mask | 2;

	output.colour.rgb =  albedo;
	output.colour.a = float(mask) / 255.0f;
	output.normal.xyz = normalize(input.normal.xyz + float3(NormalScale * n.xy, 0));;
	output.normal.w = Roughness;

    return output;
}
