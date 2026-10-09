#include "BasicSprite.hlsli"

Texture2D spriteTexture : register(t0);
Texture2D maskTexture : register(t1);
SamplerState spriteSampler : register(s0);

cbuffer CbDissolve : register(b0)
{
    float4 color;
    float amount;
    float3 padding;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 result = spriteTexture.Sample(spriteSampler, pin.texcoord) * color;
    if (amount >= 1.0f) return 0.0f;
    if (amount > 0.0f)
        result.a *= step(amount, maskTexture.Sample(spriteSampler, pin.texcoord).r);
    return result;
}
