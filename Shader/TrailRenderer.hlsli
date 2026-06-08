// TrailRenderer.hlsli

struct VS_IN
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer CbScene : register(b0)
{
    row_major float4x4 viewProjection;
};
