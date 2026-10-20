#include "TerrainGrass.hlsli"

float4 main(GrassPSInput input) : SV_TARGET
{
    float3 baseColor = shadingMode > 1.5f ? float3(0.72f, 0.72f, 0.75f) : tint.rgb;
    float3 color = baseColor * (shadingMode > 0.5f && shadingMode < 1.5f ? 1.0f : input.shade);
    if (fogEnabled > 0.5f)
    {
        const float distanceToCamera = distance(input.worldPosition, cameraPosition);
        const float fogDistance = max(fogEnd - fogStart, 0.001f);
        const float distanceFromFogStart = distanceToCamera - fogStart;
        const float fogRateBeforeStrength = saturate(distanceFromFogStart / fogDistance);
        const float fogRate = fogRateBeforeStrength * fogStrength;
        color = lerp(color, fogColor.rgb, fogRate);
    }
    return float4(color, 1.0f);
}
