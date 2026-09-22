cbuffer TerrainGrassConstants : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 viewProjection;
    float3 cameraPosition;
    float time;
    float width;
    float height;
    float windStrength;
    float windSpeed;
    float sizeVariation;
    float drawDistance;
    float terrainSize;
    float padding;
    float4 tint;
    float4 fogColor;
    float fogStart;
    float fogEnd;
    float fogStrength;
    float fogEnabled;
};

Texture2D<float4> terrainDataMap : register(t0);
SamplerState terrainPointSampler : register(s0);

struct GrassVSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float random : TEXCOORD0;
};

struct GrassGSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float random : TEXCOORD0;
};

struct GrassPSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float shade : TEXCOORD1;
};
