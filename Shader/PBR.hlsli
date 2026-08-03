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
    int3 _dummyCbMaterial;
};

static const int MaxDamageHoles = 8;

cbuffer CbDamageHoles : register(b2)
{
    float4 damageHoles[MaxDamageHoles]; // xyz: world center, w: radius
    float4 damageHoleDirections[MaxDamageHoles]; // xyz: world dent direction
    int damageHoleCount;
    float damageHoleEdgeWidth;
    float damageHoleDepth;
    float _dummyCbDamageHoles;
};

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
