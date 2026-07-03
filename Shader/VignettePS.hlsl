// VignettePS.hlsl

#include "FullScreenQuad.hlsli"

cbuffer CbVignette : register(b2)
{
    float4 color;
    float2 center;
    float intensity;
    float smoothness;
    
    float rounded;
    float roundness;
    float2 DUMMY;
};

Texture2D sceneMap : register(t0);
SamplerState linearSamplerState : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 sceneMapSize;
    sceneMap.GetDimensions(sceneMapSize.x, sceneMapSize.y);

    float4 color = sceneMap.Sample(linearSamplerState, pin.texcoord);

    //  ü•ÓŒ¸Œõˆ—
    float2 d = abs(pin.texcoord - center) * intensity;

    //  Œ¸Œõ‚ğƒXƒNƒŠ[ƒ“‚É‡‚í‚·‚©‚Ç‚¤‚©
    d.x *= lerp(1.0f, sceneMapSize.x / sceneMapSize.y, rounded);

    //  ‹÷‚Ì”Z‚³
    d = pow(saturate(d), roundness);

    half vignetteFactor = pow(saturate(1.0f - dot(d, d)), smoothness);

    color.rgb *= lerp(color.rgb, (float3) 1.0f, vignetteFactor);

    return color;
}