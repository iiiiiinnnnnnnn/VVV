// PBR.hlsli
#ifndef __PBR_HLSLI__
#define __PBR_HLSLI__

#include "ShadingFunctions.hlsli"
#include "Scene.hlsli"

struct VS_OUT
{
    float4 vertex       : SV_POSITION;
    float2 texcoord     : TEXCOORD0;
    float3 normal       : NORMAL;
    float3 position     : POSITION;
    float3 tangent      : TANGENT;
};

static const int ShadowCascadeCount = 4;

cbuffer CbShadowMap : register(b0)
{
    row_major float4x4 light_view_projections[ShadowCascadeCount];
    float4 cascade_splits;
    float4 camera_front;
    float4 shadowColor; // 影の色
    float shadowBias; // 震度バイアス
    int pcfKernelSize; // PCFカーネルサイズ
    float2 _dummyCbShadowMap;
};

cbuffer CbMaterial : register(b1)
{
    float4 baseColor;
    float4 emissiveColor;
    float4 emissionColor;
    float4 fresnelColor;

    float metalness;
    float roughness;
    float occlusion;
    float occlusionStrength;

    float shadowStrength;
    float fresnelPower;
    float fresnelStrength;
    int useMetalnessTexture;

    int useRoughnessTexture;
    int useOcclusionTexture;
    int useEmissiveTexture;
    int isFlatShading;

    int useBaseColorTexture;
    float dissolveAmount;
    float2 _dummyCbMaterial;
    float transmission;
    float indexOfRefraction;
    float refractionDistance;
    float _dummyTransmission;
};

float DissolveNoise(float2 uv)
{
    float2 cell = floor(uv * 32.0f);
    float2 blend = frac(uv * 32.0f);
    blend = blend * blend * (3.0f - 2.0f * blend);
    float4 samples = frac(sin(float4(
        dot(cell, float2(127.1f, 311.7f)),
        dot(cell + float2(1, 0), float2(127.1f, 311.7f)),
        dot(cell + float2(0, 1), float2(127.1f, 311.7f)),
        dot(cell + float2(1, 1), float2(127.1f, 311.7f)))) * 43758.5453f);
    return lerp(lerp(samples.x, samples.y, blend.x), lerp(samples.z, samples.w, blend.x), blend.y);
}

void ApplyDissolve(float2 uv)
{
    if (dissolveAmount > 0.0f) clip(DissolveNoise(uv) - dissolveAmount);
}

float DistanceFogFactor(float3 worldPosition)
{
    float distanceFromCamera = distance(viewPosition, worldPosition);
    float fogEnd = max(distanceFogParams.y, distanceFogParams.x + 0.001f);
    float fog = smoothstep(distanceFogParams.x, fogEnd, distanceFromCamera);
    float enabled = step(0.5f, distanceFogParams.w);
    return fog * fog * saturate(distanceFogParams.z) * enabled;
}

float3 DistanceFogColor()
{
    return distanceFogColor.rgb;
}

float3 ApplyDistanceFog(float3 color, float3 worldPosition)
{
    return lerp(color, DistanceFogColor(), DistanceFogFactor(worldPosition));
}

#endif // __PBR_HLSLI__
