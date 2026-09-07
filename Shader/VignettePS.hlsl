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

    float4 sceneColor = sceneMap.Sample(linearSamplerState, pin.texcoord);

    //  周辺減光処理
    float2 d = abs(pin.texcoord - center) * intensity;

    //  減光をスクリーンに合わすかどうか
    d.x *= lerp(1.0f, sceneMapSize.x / sceneMapSize.y, rounded);

    //  隅の濃さ
    d = pow(saturate(d), roundness);

    float vignetteFactor = pow(saturate(1.0f - dot(d, d)), smoothness);
    float vignetteAmount = 1.0f - vignetteFactor;

    sceneColor.rgb = lerp(sceneColor.rgb, color.rgb, vignetteAmount);

    return sceneColor;
}
