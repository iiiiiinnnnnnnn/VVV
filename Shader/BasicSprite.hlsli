// BasicSprite.hlsli

// 頂点シェーダー出力データ
struct VS_OUT
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
	float alpha : COLOR0;
};

cbuffer CbSpriteColor : register(b3)
{
    float decodeSrgb;
    float3 spriteColorPadding;
};

float4 DecodeSpriteColor(float4 value)
{
    // メタデータのない PNG も sRGB として扱い、リニアの描画先へ渡す。アルファは変換しない
    if (decodeSrgb > 0.5f)
    {
        float3 low = value.rgb / 12.92f;
        float3 high = pow(max((value.rgb + 0.055f) / 1.055f, 0.0f), 2.4f);
        value.rgb = float3(
            value.r <= 0.04045f ? low.r : high.r,
            value.g <= 0.04045f ? low.g : high.g,
            value.b <= 0.04045f ? low.b : high.b);
    }
    return value;
}
