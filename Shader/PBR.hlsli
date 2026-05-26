#ifndef __PBR_HLSLI__
#define __PBR_HLSLI__

#include "ShadingFunctions.hlsli"
#include "Scene.hlsli"

struct VS_OUT
{
    float4 vertex : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
    float3 position : POSITION;
    float3 tangent : TANGENT;
    float3 shadow_texcoord : TEXCOORD1;
};

cbuffer CbPBR : register(b0)
{
    float4 materialColor;
    float adjust_metalness;
    float adjust_roughness;
    float2 _dummyadjust;
};

// シャドウマップ用定数バッファ
cbuffer CbShadowMap : register(b1)
{
    row_major float4x4 light_view_projection;
    float shadow_attenuation;
    float shadow_bias;
    float2 shadow_dummy;
};

#endif // __PBR_HLSLI__