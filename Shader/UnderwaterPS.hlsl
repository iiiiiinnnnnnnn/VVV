#include "FullScreenQuad.hlsli"

Texture2D sceneColor : register(t0);
Texture2D sceneDepth : register(t1);
SamplerState linearClampSampler : register(s0);

cbuffer CbUnderwater : register(b2)
{
    row_major float4x4 inverseViewProjection;
    row_major float4x4 inverseWaterWorld;
    float3 cameraPosition;
    float blend;
    float4 fogColor;
    float3 absorption;
    float density;
    float tint;
    float3 padding;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 original = sceneColor.Sample(linearClampSampler, pin.texcoord);
    uint width, height;
    sceneDepth.GetDimensions(width, height);
    int2 pixel = min(int2(pin.texcoord * float2(width, height)), int2(width, height) - 1);
    float depth = sceneDepth.Load(int3(pixel, 0)).r;
    float2 ndc = pin.texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 world = mul(float4(ndc, depth, 1.0f), inverseViewProjection);
    float3 position = world.xyz / world.w;
    float3 ray = position - cameraPosition;
    float distanceInWater = length(ray);

    // 水面または水面の配置範囲を出るまでの距離に制限する。
    float3 localEye = mul(float4(cameraPosition, 1.0f), inverseWaterWorld).xyz;
    float3 localRay = mul(float4(ray, 0.0f), inverseWaterWorld).xyz;
    float exitRate = 1.0f;
    if (localRay.y > 0.000001f)
        exitRate = min(exitRate, -localEye.y / localRay.y);
    if (abs(localRay.x) > 0.000001f)
        exitRate = min(exitRate, (sign(localRay.x) * 0.5f - localEye.x) / localRay.x);
    if (abs(localRay.z) > 0.000001f)
        exitRate = min(exitRate, (sign(localRay.z) * 0.5f - localEye.z) / localRay.z);
    distanceInWater *= saturate(exitRate);

    float3 transmitted = original.rgb * exp(-max(absorption, 0.0f) * distanceInWater);
    float fog = 1.0f - exp(-max(density, 0.0f) * distanceInWater);
    float3 underwater = lerp(transmitted, fogColor.rgb, fog);
    underwater = lerp(underwater, underwater * float3(0.65f, 0.93f, 1.0f), saturate(tint));
    return float4(lerp(original.rgb, underwater, saturate(blend)), original.a);
}
