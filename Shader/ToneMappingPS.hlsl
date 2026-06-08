// ToneMappingPS.hlsl

#include "BasicSprite.hlsli"
#include "PostEffect.hlsli"
#include "ShadingFunctions.hlsli"

Texture2D colorMap : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = colorMap.Sample(linearSampler, pin.texcoord);

    // ─── 彩度調整 ───
    float lum = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    color.rgb = lerp(lum, color.rgb, saturation);

    // ─── 露出調整 ───
    color.rgb *= exposure;

    // ─── ACESトーンマッピング ───
    color.rgb = (color.rgb * (2.51 * color.rgb + 0.03))
              / (color.rgb * (2.43 * color.rgb + 0.59) + 0.14);
    color.rgb = saturate(color.rgb);

    // ガンマ補正
    color.rgb = pow(max(color.rgb, 0.0f), 1.0f / GammaFactor);
    
    return color;
}