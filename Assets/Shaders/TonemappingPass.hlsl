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

cbuffer ZoomConstantBuffer : register(b0)
{
	float4 RTSize;
	float4 ZoomConfig;
}

Texture2D<float4> mainBuffer : register(t0);

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

	output.rgb = pow ( abs(result), 1/2.2);

	return output;
}
