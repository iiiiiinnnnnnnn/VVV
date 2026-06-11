// ToneMappingPS.hlsl

#include "BasicSprite.hlsli"
#include "PostEffect.hlsli"
#include "ShadingFunctions.hlsli"

Texture2D colorMap : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = colorMap.Sample(linearSampler, pin.texcoord);

	// HDR値のマイナス事故防止
    color.rgb = max(color.rgb, 0.0f);

	// 露出調整
    color.rgb *= exposure;

	// ACESトーンマッピング
    color.rgb = (color.rgb * (2.51f * color.rgb + 0.03f))
			  / (color.rgb * (2.43f * color.rgb + 0.59f) + 0.14f);

    color.rgb = saturate(color.rgb);

	// ガンマ補正
    color.rgb = pow(color.rgb, 1.0f / GammaFactor);

	// 彩度調整
    float lum = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    color.rgb = lerp(lum.xxx, color.rgb, saturation);

    color.rgb = saturate(color.rgb);

    return color;
}