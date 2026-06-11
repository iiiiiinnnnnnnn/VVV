// BloomPS.hlsl

#include "BasicSprite.hlsli"
#include "PostEffect.hlsli"

Texture2D colorMap : register(t0);
Texture2D luminanceMap : register(t1);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    uint width, height;
    luminanceMap.GetDimensions(width, height);

    float4 color = colorMap.Sample(linearSampler, pin.texcoord);

    float3 blurColor = 0.0f;
    float gaussianKernelTotal = 0.0f;

    const float PI = 3.14159265358979f;
    const int gaussianHalfKernelSize = 3;

    float sigma = max(gaussianSigma, 0.0001f);

    [unroll]
    for (int x = -gaussianHalfKernelSize; x <= gaussianHalfKernelSize; x++)
    {
        [unroll]
        for (int y = -gaussianHalfKernelSize; y <= gaussianHalfKernelSize; y++)
        {
            float gaussianKernel =
                exp(-(x * x + y * y) / (2.0f * sigma * sigma)) /
                (2.0f * PI * sigma * sigma);

            float2 uv = pin.texcoord + float2(
                x * 1.0f / width,
                y * 1.0f / height
            );

            blurColor += luminanceMap.Sample(linearSampler, uv).rgb * gaussianKernel;
            gaussianKernelTotal += gaussianKernel;
        }
    }

    blurColor /= max(gaussianKernelTotal, 0.0001f);

    color.rgb += blurColor * bloomIntensity;

    return color;
}