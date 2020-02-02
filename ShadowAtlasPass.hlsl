

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

cbuffer ShadowTileCB : register(b0)
{
	uint ShadowTileIndex;
};

static const int MaxNoofShadowCasters = 10;

cbuffer ShadowAtlasPassCB : register(b1)
{
	float4x4 ViewProjection[MaxNoofShadowCasters * 6];
};

cbuffer perModelInstanceCB : register(b2)
{
	float4x4	World;
	float		AlbedoID;
	float		NormalID;
	float		Roughness;
	float		Metalness;
	float4		Colour;
	float2		UVscale;
	float2		NormalScale;
	float		Emissive;
	float3		perModelInstanceCB_pad;
};

float4 VSMain(VSInput input) : SV_POSITION
{
	float4 position = mul(World, float4(input.position.xyz, 1));
	position = mul(ViewProjection[ShadowTileIndex], position);

    return position;
}


