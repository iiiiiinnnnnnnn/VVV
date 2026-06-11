// LuminanceExtractionPS.hlsl

#include "BasicSprite.hlsli"
#include "PostEffect.hlsli"

Texture2D colorMap : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = colorMap.Sample(linearSampler, pin.texcoord);

    float luminance = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));

    float edgeRange = max(
        luminanceExtractionHigherEdge - luminanceExtractionLowerEdge,
        0.0001f
    );

    float mask = saturate(
        (luminance - luminanceExtractionLowerEdge) / edgeRange
    );

    color.rgb *= mask;

    return color;
}