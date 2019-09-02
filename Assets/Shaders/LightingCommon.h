
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

float3 LightingGGX(float3 N, float3 V, float3 L, float roughness, float3 F0)
{
	float alpha = roughness*roughness;

	float3 H = normalize(V + L);

	float dotNL = saturate(dot(N, L));
	float dotNV = saturate(dot(N, V));
	float dotNH = saturate(dot(N, H));
	float dotLH = saturate(dot(L, H));

	// D
	float alphaSqr = alpha*alpha;
	float pi = 3.14159f;
	float denom = dotNH * dotNH *(alphaSqr - 1.0) + 1.0f;
	float D = alphaSqr / (pi * denom * denom);

	// F
	float3 F = Fresnel(F0, L, H);

	// V
	float k = alpha / 2.0f;
	float Vis = G1V(dotNL, k)*G1V(dotNV, k);

	float3 specular = (dotNL * D * Vis) * F;

	return specular;
}

