// BloomPS.hlsl

#include "BasicSprite.hlsli"
#include "PostEffect.hlsli"

Texture2D colorMap : register(t0);
Texture2D bloomMap : register(t1);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = colorMap.Sample(linearSampler, pin.texcoord);
    float3 bloomColor = bloomMap.Sample(linearSampler, pin.texcoord).rgb;

    color.rgb += bloomColor * bloomIntensity;

    return color;
}