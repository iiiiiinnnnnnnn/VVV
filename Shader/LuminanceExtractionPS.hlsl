// LuminanceExtractionPS.hlsl

#include "BasicSprite.hlsli"
#include "PostEffect.hlsli"

Texture2D colorMap : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = colorMap.Sample(linearSampler, pin.texcoord);

    color.rgb = max(color.rgb, 0.0f);

    float luminance = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));

    float mask = smoothstep(
        luminanceExtractionLowerEdge,
        luminanceExtractionHigherEdge,
        luminance
    );

    float3 bloomColor = color.rgb * mask;

    // 小さい白飛びピクセルがBloomで四角く光るのを防ぐ
    // 値を上げるほど強いBloomを許す
    const float maxBloomLuminance = 2.0f;

    float bloomLuminance = dot(bloomColor, float3(0.299f, 0.587f, 0.114f));

    if (bloomLuminance > maxBloomLuminance)
    {
        bloomColor *= maxBloomLuminance / bloomLuminance;
    }

    return float4(bloomColor, color.a);
}