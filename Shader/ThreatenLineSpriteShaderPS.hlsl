// ThreatenLineSpriteShaderPS.hlsl

#include "BasicSprite.hlsli"

cbuffer CbThreaten : register(b2)
{
    float4 color;

    float2 center;
    float screenAspect;
    float lineCount;

    float lineWidth;
    float softness;
    float innerRadius;
    float outerRadius;

    float randomStrength;
    float randomSeed;
    float rotation;
    float alphaMultiplier;

    float time;
    float randomChangeSpeed;
    float rotationSpeed;
    float noiseScroll;
};

static float Hash11(float x)
{
    return frac(sin(x * 127.1f) * 43758.5453f);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    float2 p = uv - center;
    p.x *= screenAspect;

    float dist = length(p);
    float angle = atan2(p.y, p.x) + rotation + time * rotationSpeed;

    float angle01 = frac(angle / 6.2831853f);

    float slot = floor(angle01 * lineCount);

    // 時間でランダムパターンを切り替える
    float randomFrame = floor(time * randomChangeSpeed);

    // スロットごとのランダム
    float rnd = Hash11(slot + randomSeed * 31.7f + randomFrame * 113.1f);

    float randomMask = lerp(1.0f, rnd, randomStrength);

    float patternOffset = Hash11(randomFrame + randomSeed) * 6.2831853f;

    float wave = abs(sin(angle01 * lineCount * 3.14159265f + patternOffset));

    float lineMask = 1.0f - smoothstep(lineWidth, lineWidth + softness, wave);

    float radialIn = smoothstep(innerRadius, innerRadius + 0.15f, dist);
    float radialOut = 1.0f - smoothstep(outerRadius, outerRadius + 0.08f, dist);
    float radial = radialIn * radialOut;

    float edgeBoost = smoothstep(0.25f, outerRadius, dist);

    float alpha = lineMask * radial * edgeBoost * randomMask * color.a * alphaMultiplier;

    return float4(color.rgb, alpha);
}