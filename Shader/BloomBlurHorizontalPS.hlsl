// BloomBlurHorizontalPS.hlsl

#include "BasicSprite.hlsli"
#include "PostEffect.hlsli"

Texture2D colorMap : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    uint width, height;
    colorMap.GetDimensions(width, height);

    float blurRadius = max(gaussianSigma, 0.0f);
    float2 texelSize = float2(1.0f / (float) width, 0.0f);

    float3 color = colorMap.Sample(linearSampler, pin.texcoord).rgb * 0.2270270270f;

    color += colorMap.Sample(linearSampler, pin.texcoord + texelSize * (1.3846153846f * blurRadius)).rgb * 0.3162162162f;
    color += colorMap.Sample(linearSampler, pin.texcoord - texelSize * (1.3846153846f * blurRadius)).rgb * 0.3162162162f;

    color += colorMap.Sample(linearSampler, pin.texcoord + texelSize * (3.2307692308f * blurRadius)).rgb * 0.0702702703f;
    color += colorMap.Sample(linearSampler, pin.texcoord - texelSize * (3.2307692308f * blurRadius)).rgb * 0.0702702703f;

    return float4(color, 1.0f);
}