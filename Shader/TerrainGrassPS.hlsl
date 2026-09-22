#include "TerrainGrass.hlsli"

float4 main(GrassPSInput input) : SV_TARGET
{
    float3 color = tint.rgb * input.shade;
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
