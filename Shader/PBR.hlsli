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
    float4 baseColor;           // ベースカラー
    float4 emissiveColor;       // エミッシブカラー
    float  metalness;           // メタルネス
    float  roughness;           // ラフネス
    float  occlusionStrength;   // AO強度
    bool   hasMetalRoughTexture; // メタルネスラフネステクスチャの有無
    bool   isFace;
    float3 _dummyCbMaterial;
};

#endif // __PBR_HLSLI__
