// GeometryParticlePS.hlsl

#include "GeometryParticle.hlsli"

Texture2D colorMap : register(t0);
SamplerState colorSampler : register(s0);

float4 main(PS_IN pin) : SV_TARGET0
{
    float4 tex = colorMap.Sample(colorSampler, pin.texcoord);

    // PNGの完全透明部分はブレンドへ回さず、四角い背景色を防ぐ。
    clip(tex.a - (1.0 / 255.0));

    return float4(tex.rgb * pin.color.rgb, tex.a * pin.color.a);
}
