// ChromaticAberrationPS.hlsl

#include "FullScreenQuad.hlsli"

cbuffer CbChromaticAberration : register(b2)
{
    float amount;
    int maxSamples;
    float2 DUMMY;
    
    float4 shift[3];
};

Texture2D sceneMap : register(t0);
SamplerState linearSamplerState : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 sceneMapSize;
    sceneMap.GetDimensions(sceneMapSize.x, sceneMapSize.y);
    float2 sceneMapTexelSize = (float2) 1.0f / sceneMapSize.xy;

    float2 coords = 2.0f * pin.texcoord - 1.0f;
    float2 end = pin.texcoord - coords * dot(coords, coords) * amount;

    float2 diff = end - pin.texcoord;
    int samples = clamp(int(length(sceneMapTexelSize * diff / 2.0f)), 3, maxSamples);
    float2 delta = diff / samples;
    float2 pos = pin.texcoord;
    float4 sum = (0.0f).xxxx, filterSum = (0.0f).xxxx;
    for (int index = 0; index < maxSamples; ++index)
    {
        int t = (int) (index + 0.5f) / samples;
        float4 s = sceneMap.Sample(linearSamplerState, pos);
        float3 rgb = shift[t % 3];
        float4 filter = float4(rgb, 1.0f);

        sum += s * filter;
        filterSum += filter;
        pos += delta;
    }

    return sum / filterSum;
}