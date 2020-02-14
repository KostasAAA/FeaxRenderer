
#define PI 3.14159265359f
#define ONE_OVER_PI (1.0/PI)

struct DirectionalLightData
{
	float4	Direction;
	float4	Colour;
	float Intensity;
	float3 DirectionalLightData_pad;
};

struct PointLightData
{
	float4	Position;
	float4	Colour;
	float4	ShadowmapRange;
	float	Intensity;
	float	Attenuation;
	float	Radius;
	float	PointLightData_pad;
};

float3 Fresnel(float3 F0, float3 L, float3 H)
{
	float dotLH = saturate(dot(L, H));
	float dotLH5 = pow(1.0f - dotLH, 5);
	return F0 + (1.0 - F0)*(dotLH5);
}

float G1V(float dotNV, float k)
{
	return 1.0f / (dotNV*(1.0f - k) + k);
}

float3 SpecularBRDF(float3 N, float3 V, float3 L, float roughness, float3 F0)
{
	float alpha = roughness*roughness;

	float3 H = normalize(V + L);

	float dotNL = saturate(dot(N, L));
	float dotNV = saturate(dot(N, V));
	float dotNH = saturate(dot(N, H));
	float dotLH = saturate(dot(L, H));

	// D, based on GGX
	float alphaSqr = alpha*alpha;
	float denom = dotNH * dotNH *(alphaSqr - 1.0) + 1.0f;
	float D = alphaSqr / (PI * denom * denom);

	// F
	float3 F = Fresnel(F0, L, H);

	// V
	float k = alpha / 2.0f;
	float Vis = G1V(dotNL, k)*G1V(dotNV, k);

	float3 specular = ( D * Vis) * F;

	return specular;
}

float DiffuseBRDF()
{
	return ONE_OVER_PI;
}

float SquareFalloffAttenuation(float3 posToLight, float lightInvRadius) 
{
	float distanceSquare = dot(posToLight, posToLight);

	//return 1 / max(distanceSquare, 1e-4);

	float factor = distanceSquare * lightInvRadius * lightInvRadius;
	float smoothFactor = max(1.0 - factor * factor, 0.0);
	return (smoothFactor * smoothFactor) / max(distanceSquare, 1e-4);
}