#include "PBR.hlsli"

Texture2D baseMap : register(t0);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT input) : SV_TARGET
{
    float4 textureColorSRGB = useBaseColorTexture != 0
        ? baseMap.Sample(linearSampler, input.texcoord)
        : float4(1.0f, 1.0f, 1.0f, 1.0f);

    float4 textureColor = float4(
        pow(textureColorSRGB.rgb, GammaFactor),
        textureColorSRGB.a);

    float4 color = textureColor * baseColor;
    color.rgb += emissionColor.rgb * emissionColor.a;
    return color;
}
