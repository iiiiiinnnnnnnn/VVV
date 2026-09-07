// VignetteSpritePS.hlsl

#include "BasicSprite.hlsli"

Texture2D spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);

cbuffer CbVignette : register(b2)
{
    float4 vignetteColor;
    float2 textureSize;
    float vignetteRange;
    float vignetteSoftness;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    const float2 safeSize = max(textureSize, 1.0f.xx);
    const float2 uv = saturate(pin.texcoord);

    // 画面のアスペクト比ではなく、このスプライトの画像端からの
    // ピクセル距離で形を決める。横長画像でも中央を急に狭めない。
    const float2 pixelPosition = uv * safeSize;
    const float2 distanceToEdge = min(pixelPosition, safeSize - pixelPosition);
    const float halfShortSide = max(min(safeSize.x, safeSize.y) * 0.5f, 1.0f);
    const float nearestEdge = min(distanceToEdge.x, distanceToEdge.y) / halfShortSide;

    const float range = clamp(vignetteRange, 0.001f, 1.0f);
    const float softness = clamp(vignetteSoftness, 0.001f, range);
    const float solidEnd = max(range - softness, 0.0f);
    const float vignette = 1.0f - smoothstep(solidEnd, range, nearestEdge);

    float4 finalColor = spriteTexture.Sample(spriteSampler, uv);
    const float amount = vignette * saturate(vignetteColor.a);
    finalColor.rgb = lerp(finalColor.rgb, vignetteColor.rgb, amount);
    return finalColor;
}
