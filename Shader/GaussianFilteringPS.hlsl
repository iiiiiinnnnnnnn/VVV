// GaussianFilteringPS.hlsl

#include "BasicSprite.hlsli"

//  カーネル最大サイズ
static const int KernelMax = 25;

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

//  定数バッファ
cbuffer GAUSSIAN_FILTER : register(b2)
{
    float4 weights[KernelMax * KernelMax];
    float kernel_size;
    float2 texcel;
    float dummy;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = (float4) 0;
    color.a = 1;
    //  指定のカーネルサイズ分周囲から色を取得。CPU側で計算した重みを積和していく
    for (int i = 0; i < kernel_size * kernel_size; i++)
    {
        float2 offset = texcel * weights[i].xy;
        float weight = weights[i].z;
        color.rgb += texture0.Sample(sampler0, pin.texcoord + offset).rgb * weight;
    }
    
    return color;
}
