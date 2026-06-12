// TerrainPrimitive.hlsli

#ifndef __TERRAIN_PRIMITIVE_HLSLI__
#define __TERRAIN_PRIMITIVE_HLSLI__

#include "PBR.hlsli"

struct HS_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct HS_CONSTANT_OUT
{
    float edge[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

#define HS_OUT HS_IN
#define DS_IN HS_IN

cbuffer CbTerrainTessellation : register(b2)
{
    float edge_factor;
    float inner_factor;
    float height_scaler;
    float tilling_scale;
};

cbuffer CbTerrainObject : register(b3)
{
    row_major float4x4 world;
    float terrain_size;
    float height_map_texel_size;
    float2 object_dummy;
};

Texture2D<float4> terrainDataMap : register(t0);

SamplerState terrainPointClampSampler : register(s0);
SamplerState shadowSampler : register(s1);
SamplerState linearSampler : register(s2);

#endif
