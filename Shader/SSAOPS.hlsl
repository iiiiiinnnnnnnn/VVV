// SSAOPS.hlsl

#include "FullScreenQuad.hlsli"

Texture2D sceneDepthMap : register(t0);
SamplerState linearSamplerState : register(s0);

cbuffer CbSSAO : register(b2)
{
    row_major float4x4 viewTransform;
    row_major float4x4 inverseViewProjectionTransform;
    row_major float4x4 projectionTransform;
    float4 zBufferParameteres;

    float radius;
    float intensity;
    float minDistance;
    float maxDistance;
};

static const int sampleCount = 16;
static const float2 sampleOffsets[sampleCount] =
{
    float2( 0.0000f,  1.0000f),
    float2( 0.7071f,  0.7071f),
    float2( 1.0000f,  0.0000f),
    float2( 0.7071f, -0.7071f),
    float2( 0.0000f, -1.0000f),
    float2(-0.7071f, -0.7071f),
    float2(-1.0000f,  0.0000f),
    float2(-0.7071f,  0.7071f),
    float2( 0.3827f,  0.9239f),
    float2( 0.9239f,  0.3827f),
    float2( 0.9239f, -0.3827f),
    float2( 0.3827f, -0.9239f),
    float2(-0.3827f, -0.9239f),
    float2(-0.9239f, -0.3827f),
    float2(-0.9239f,  0.3827f),
    float2(-0.3827f,  0.9239f),
};

float3 ReconstructWorldPosition(float2 texcoord, float depth)
{
    float2 ndc = texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 world = mul(float4(ndc, depth, 1.0f), inverseViewProjectionTransform);
    return world.xyz / world.w;
}

float GetViewDepth(float2 texcoord)
{
    float depth = sceneDepthMap.SampleLevel(linearSamplerState, texcoord, 0).r;
    float3 worldPosition = ReconstructWorldPosition(texcoord, depth);
    float4 viewPosition = mul(float4(worldPosition, 1.0f), viewTransform);
    return viewPosition.z;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 sceneMapSize;
    sceneDepthMap.GetDimensions(sceneMapSize.x, sceneMapSize.y);

    float depth = sceneDepthMap.SampleLevel(linearSamplerState, pin.texcoord, 0).r;
    if (depth >= 0.9999f)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    float centerDepth = GetViewDepth(pin.texcoord);
    float texelRadius = max(radius, 0.0f) / max(centerDepth, 0.0001f);
    texelRadius = min(texelRadius, 64.0f) / sceneMapSize.y;

    float occlusion = 0.0f;
    for (int index = 0; index < sampleCount; ++index)
    {
        float scale = (index + 1.0f) / sampleCount;
        float2 sampleTexcoord = pin.texcoord + sampleOffsets[index] * texelRadius * scale;
        float sampleDepthValue = sceneDepthMap.SampleLevel(linearSamplerState, sampleTexcoord, 0).r;
        if (sampleDepthValue >= 0.9999f)
        {
            continue;
        }

        float sampleDepth = GetViewDepth(sampleTexcoord);
        float delta = centerDepth - sampleDepth;
        float range = saturate(1.0f - abs(delta) / max(maxDistance, 0.0001f));
        occlusion += (delta > minDistance && delta < maxDistance) ? range : 0.0f;
    }

    occlusion /= sampleCount;
    float ao = saturate(1.0f - occlusion * intensity);
    return float4(ao, ao, ao, 1.0f);
}
