// TerrainPrimitive.hlsli

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

struct DS_OUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
};

cbuffer CbTerrainObject : register(b0)
{
    row_major float4x4 world;
    float terrain_size;
    float height_map_texel_size;
    float2 object_dummy;
};

cbuffer CbTerrainTessellation : register(b2)
{
    float edge_factor;
    float inner_factor;
    float height_scaler;
    float tilling_scale;
};

cbuffer CbTerrainScene : register(b7)
{
    row_major float4x4 viewProjection;
    float3 viewPosition;
    float scene_dummy0;

    float3 directionalLightDirection;
    float scene_dummy1;

    float4 directionalLightColor;
    float4 ambientColor;
};

Texture2D<float4> terrainDataMap : register(t0);

SamplerState pointClampSampler : register(s0);
SamplerState linearClampSampler : register(s1);
SamplerState linearWrapSampler : register(s2);