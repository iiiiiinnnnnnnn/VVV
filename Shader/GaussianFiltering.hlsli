// GaussianFiltering.hlsli

#include "BasicSprite.hlsli"

//  カーネル最大サイズ
static const int KernelMax = 25;

//  定数バッファ
cbuffer GAUSSIAN_FILTER : register(b2)
{
    float4 weights[KernelMax * KernelMax];
    float kernel_size;
    float2 texcel;
    float dummy;
};
