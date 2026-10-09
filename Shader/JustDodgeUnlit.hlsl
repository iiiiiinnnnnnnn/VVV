#include "PBR.hlsli"

cbuffer CbJustDodgeFade : register(b3)
{
    float elapsedTime;
    float lifetime;
    float fadeInDuration;
    float fadeOutDuration;
};

// 残像

float4 main(VS_OUT input) : SV_TARGET
{
    ApplyDissolve(input.texcoord);
    const float fadeIn = smoothstep(0.0f, max(fadeInDuration, 0.0001f), elapsedTime);
    const float fadeOut = 1.0f - smoothstep(max(lifetime - fadeOutDuration, 0.0f),
        max(lifetime, 0.0001f), elapsedTime);
    const float3 color = baseColor.rgb + emissionColor.rgb * emissionColor.a;
    return float4(color, baseColor.a * fadeIn * fadeOut);
}
