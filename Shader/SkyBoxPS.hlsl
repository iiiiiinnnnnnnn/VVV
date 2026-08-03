// SkyBoxPS.hlsl

// IBLのスペキュラーキューブマップをスカイとして表示する

TextureCube  skyTex       : register(t0);  // specular_pmrem (mip0 = 高解像度)
SamplerState linearSampler : register(s0);

// スカイボックス専用定数バッファ
cbuffer CbSkyBox : register(b0)
{
    row_major float4x4 inverseViewProjection;
    float3 viewPos;
    float  skyIntensity;    // 明るさスケール
    float4 distanceFogColor;
    float4 distanceFogParams;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 ndcXY    : TEXCOORD0;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    // NDC -> ワールド方向
    float4 nearWorld = mul(float4(pin.ndcXY,  0.0f, 1.0f), inverseViewProjection);
    float4 farWorld  = mul(float4(pin.ndcXY,  1.0f, 1.0f), inverseViewProjection);
    nearWorld /= nearWorld.w;
    farWorld  /= farWorld.w;

    float3 dir = normalize(farWorld.xyz - nearWorld.xyz);

    // mip0 でサンプリング（ぼかしなし）
    float3 color = skyTex.SampleLevel(linearSampler, dir, 0).rgb;
    color *= skyIntensity;
	float fog = saturate(distanceFogParams.x) * step(0.5f, distanceFogParams.y);
	color = lerp(color, distanceFogColor.rgb, fog);

    return float4(color, 1.0f);
}
