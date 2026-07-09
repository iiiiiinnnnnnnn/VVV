// GeometryParticle.hlsli

//  í∏ì_èÓïÒ
struct VS_IN
{
    float3 position : POSITION;
    float2 size : TEXCOORD;
    float4 color : COLOR;
    float4 param : PARAMETER;
};

struct GS_IN
{
    float3 position : POSITION;
    float2 size : TEXCOORD;
    float4 color : COLOR;
    float4 param : PARAMETER;
};

struct PS_IN
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
};

cbuffer GEOMETRY_PARTICLE_DATA : register(b0)
{
    row_major float4x4 viewProjection;
    float3 cameraRight;
    float dummy0;
    float3 cameraUp;
    float dummy1;
};