#include "PBR.hlsli"

cbuffer CbRainbowData : register(b2)
{
    float lineWidth;
    float speed;
    float2 padding;
};

float3 GetRainbowColor(float time, float x)
{
    float lineIndex = floor((x - time * speed) / max(lineWidth, 0.001f));

    float hue = frac(lineIndex / 6.0);

    float3 rgb = abs(frac(hue + float3(0.0, 0.666667, 0.333333)) * 6.0 - 3.0);

    return saturate(rgb - 1.0);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float3 rainbow = GetRainbowColor(timeData.time, pin.position.x);
    return float4(rainbow, 1.0f);
}