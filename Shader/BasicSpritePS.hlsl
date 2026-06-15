// BasicSpritePS.hlsl

#include "BasicSprite.hlsli"

Texture2D spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);

cbuffer CbBasic : register(b0)
{
    float4 color;
};

// ピクセルシェーダーエントリポイント
float4 main(VS_OUT pin) : SV_TARGET
{
	float4 final = spriteTexture.Sample(spriteSampler, pin.texcoord);
    final *= color;
    return final;
}
