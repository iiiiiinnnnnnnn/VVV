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
    float3 position     : POSITION;     // ワールド座標（ライティング計算用）
    float3 tangent      : TANGENT;
    float3 shadowTexcoord : TEXCOORD1;  // シャドウマップ参照用UV+深度
};

cbuffer CbShadowMap : register(b0)
{
    row_major float4x4 light_view_projection;
    float4 shadowColor; // 影の色
    float shadowBias; // 深度オフセット
    int pcfKernelSize; // PCFカーネルサイズ
    float2 _dummyCbShadowMap;
};

cbuffer CbMaterial : register(b1)
{
    float4 baseColor;
    float4 emissiveColor;

    float metalness;
    float roughness;
    float occlusion;
    float occlusionStrength;
    
    float shadowStrength;
    int useMetalnessTexture;
    int useRoughnessTexture;
    int useOcclusionTexture;

    int isFlatShading;
    float3 _dummyCbMaterial;
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
    const float fogStart = 85.0f;
    const float fogEnd = 360.0f;

    float distanceFromCamera = distance(viewPosition, worldPosition);
    float fog = smoothstep(fogStart, fogEnd, distanceFromCamera);
    return fog * fog * 0.82f;
}

float3 DistanceFogColor()
{
    const float3 skyFogColor = float3(0.76f, 0.91f, 1.0f);
    float3 ambient = lightData.ambientColor.rgb * lightData.ambientColor.a;
    return lerp(ambient, skyFogColor, 0.65f);
}

float3 ApplyDistanceFog(float3 color, float3 worldPosition)
{
    return lerp(color, DistanceFogColor(), DistanceFogFactor(worldPosition));
}

#endif // __PBR_HLSLI__
