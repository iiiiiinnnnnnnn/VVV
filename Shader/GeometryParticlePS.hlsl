// GeometryParticlePS.hlsl

#include "GeometryParticle.hlsli"

Texture2D colorMap : register(t0);

#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
SamplerState samplerStates[3] : register(s0);

float4 main(PS_IN pin) : SV_TARGET0
{
    float4 tex = colorMap.Sample(samplerStates[ANISOTROPIC], pin.texcoord);
    float intensity = max(tex.r, max(tex.g, tex.b));
    return float4(pin.color.rgb * intensity, tex.a * pin.color.a);
}
