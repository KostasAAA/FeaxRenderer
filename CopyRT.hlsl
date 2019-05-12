
Texture2D<float4> Input : register(t0);
RWTexture2D<unorm float4> Output : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    Output[threadID.xy] = Input[threadID.xy];
}
