#include "Common.hlsl"

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

PSInput VSMain(VSInput input)
{
	PSInput result;

	result.position = float4(input.position.xyz, 1);
	result.uv = input.uv;

	return result;
}

cbuffer TonemappingConstantBuffer : register(b0)
{
	float4 RTSize;
	float4 ZoomConfig;
	float Exposure;
	float Aperture;
	float ShutterSpeed;
	float ISO;
}

Texture2D<float4> mainBuffer : register(t0);

// Computes the camera's EV100 from exposure settings
// aperture in f-stops
// shutterSpeed in seconds
// sensitivity in ISO
float GetExposureSettings(float aperture, float shutterSpeed, float sensitivity) {
	return log2((aperture * aperture) / shutterSpeed * 100.0 / sensitivity);
}

// Computes the exposure normalization factor from
// the camera's EV100
float GetExposure(float ev100)
{
	return 1.0 / (pow(2.0, ev100) * 1.2);
}

float4 PSMain(PSInput input) : SV_Target
{
	float4 output = 0;

	float3 result = mainBuffer[input.position.xy].rgb;

	float zoomFactor = int(ZoomConfig.y);
	float2 zoomWindowSize = 0.5 * RTSize.xy;

	if (ZoomConfig.x > 0 && all( input.position.xy > zoomWindowSize) )
	{
		float2 zoomPos = input.position.xy - 0.75 * RTSize.xy;
		zoomPos /= zoomFactor;
		zoomPos += 0.5 * RTSize.xy ;

		result = mainBuffer[zoomPos].rgb;

		if (ZoomConfig.z > 0 && ( int(input.position.x) % int(zoomFactor) == 0 || int(input.position.y) % int(zoomFactor) == 0 ))
			result = float3(1, 0, 0);
	}

	//float ev100 = GetExposureSettings(Aperture, ShutterSpeed, ISO);
	//float exposure = GetExposure(ev100);

//	result *= exposure;
//	result *= Exposure;

	output.rgb = accurateLinearToSRGB(abs(result.rgb));

	return output;
}
