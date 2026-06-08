// LuminanceExtractionPS.hlsl

#include "BasicSprite.hlsli"
#include "PostEffect.hlsli"

Texture2D colorMap : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = colorMap.Sample(linearSampler, pin.texcoord);
    color.rgb *= smoothstep(luminanceExtractionLowerEdge, luminanceExtractionHigherEdge, dot(color.rgb, float3(0.299f, 0.587f, 0.114f)));
    return color;
}