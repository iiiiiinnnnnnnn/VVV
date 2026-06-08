// VignetteSpritePS.hlsl

#include "BasicSprite.hlsli"

cbuffer CbVignette : register(b2)
{
    float4 vignetteColor;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord * 2.0 - 1.0;
    float dist = length(uv);
    float vignette = smoothstep(0.4, 1.0, dist);

    float4 color = 0;
    color.rgb = vignetteColor.rgb;
    color.a = vignette * vignetteColor.a;

    return color;
}