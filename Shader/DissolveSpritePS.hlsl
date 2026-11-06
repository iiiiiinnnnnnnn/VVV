#include "BasicSprite.hlsli"

Texture2D spriteTexture : register(t0);
Texture2D maskTexture : register(t1);
SamplerState spriteSampler : register(s0);

cbuffer CbDissolve : register(b0)
{
    float4 color;
    float amount;
    float maskScale;
    float softness;
    float padding;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 result = spriteTexture.Sample(spriteSampler, pin.texcoord) * color;
    if (amount >= 1.0f) return 0.0f;
    if (amount > 0.0f)
    {
        float2 maskUV = (pin.texcoord - 0.5f) * maskScale + 0.5f;
        float noise = maskTexture.Sample(spriteSampler, maskUV).r;
        if (softness > 0.0f)
        {
            float threshold = lerp(-softness, 1.0f + softness, amount);
            result.a *= smoothstep(threshold - softness, threshold + softness, noise);
        }
        else result.a *= step(amount, noise);
    }
    result.a *= pin.alpha;
    return result;
}
