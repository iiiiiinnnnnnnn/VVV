// RadialBlurPS.hlsl

#include "FullScreenQuad.hlsli"

cbuffer CbRadialBlur : register(b2)
{
    float radius;
    int samplingCount;
    float2 center;
    
    float maskRadius;
    float3 DUMMY;
};

Texture2D sceneMap : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 sceneMapSize;
    sceneMap.GetDimensions(sceneMapSize.x, sceneMapSize.y);

    float4 color = sceneMap.Sample(linearSampler, pin.texcoord);
    float4 resultColor = color;

    float2 blurVector = (center - pin.texcoord);
    blurVector *= (radius / sceneMapSize.xy) / samplingCount;
    for (int index = 1; index < samplingCount; ++index)
    {
        resultColor += sceneMap.Sample(linearSampler, pin.texcoord + blurVector * index);
    }

    //  Žw’è‚Ì”ÍˆÍ“à‚Í“K‰ž—Ê‚ð•Ï‚¦‚é
    float maskValue = saturate(
    (length(pin.texcoord - center)) /
    (maskRadius / min(sceneMapSize.x, sceneMapSize.y)));
    
    return lerp(color, resultColor / samplingCount, maskValue);
}


