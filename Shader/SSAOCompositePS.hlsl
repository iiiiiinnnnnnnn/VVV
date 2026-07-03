// SSAOCompositePS.hlsl

#include "FullScreenQuad.hlsli"

Texture2D sceneMap : register(t0);
Texture2D ssaoMap : register(t1);
SamplerState linearSamplerState : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = sceneMap.Sample(linearSamplerState, pin.texcoord);
    float ao = ssaoMap.Sample(linearSamplerState, pin.texcoord).r;
    color.rgb *= ao;
    return color;
}
