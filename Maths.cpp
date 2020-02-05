
#include "stdafx.h"

XMVECTOR xyY_to_XYZ(XMVECTOR& v) 
{
	return XMVectorSet( v.m128_f32[0] / v.m128_f32[1], v.m128_f32[2], (1.0f - v.m128_f32[0] - v.m128_f32[1]) / v.m128_f32[1] , 0);
}

XMVECTOR XYZ_to_sRGB(XMVECTOR const& v) 
{
	// XYZ to linear sRGB
	const XMMATRIX XYZ_sRGB
	(
		3.2404542f, -0.9692660f, 0.0556434f, 0,
		-1.5371385f, 1.8760108f, -0.2040259f, 0,
		-0.4985314f, 0.0415560f, 1.0572252f, 0,
		0, 0, 0, 1
	);
	return XMVector3Transform(v, XYZ_sRGB);
}

XMVECTOR ConvertKelvinToLinearRGB (float K)
{
	// temperature to CIE 1960
	float K2 = K * K;
	float u = (0.860117757f + 1.54118254e-4f * K + 1.28641212e-7f * K2) /
		(1.0f + 8.42420235e-4f * K + 7.08145163e-7f * K2);
	float v = (0.317398726f + 4.22806245e-5f * K + 4.20481691e-8f * K2) /
		(1.0f - 2.89741816e-5f * K + 1.61456053e-7f * K2);

	float d = 1.0f / (2.0f * u - 8.0f * v + 4.0f);

	XMVECTOR linear = XYZ_to_sRGB( xyY_to_XYZ( XMVectorSet( 3.0f * u * d, 2.0f * v * d, 1.0f , 0) ) );

	float maxComponent = XMVector3MaxElement(linear);

	linear = linear / maxComponent;

	// normalize and saturate
	return linear;// saturate(linear / std::max(1e-5f, max(linear)));
}