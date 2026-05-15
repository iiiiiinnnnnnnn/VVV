// ShadowmapFunctions.hlsli
// Ex04_Base から移植 (2026-05-15)

// ────────────────────────────────────────────────────────────────────────────
//  ワールド座標 → シャドウマップ参照用 UV + 深度
// ────────────────────────────────────────────────────────────────────────────
float3 CalcShadowTexcoord(float3 worldPosition, matrix lightViewProjection)
{
    float4 p = mul(float4(worldPosition, 1), lightViewProjection);
    p /= p.w;
    p.y = -p.y;
    p.xy = 0.5f * p.xy + 0.5f;
    return p.xyz;
}

// ────────────────────────────────────────────────────────────────────────────
//  シンプルシャドウ判定
// ────────────────────────────────────────────────────────────────────────────
float3 CalcShadowColor(Texture2D tex, SamplerState samplerState,
                       float3 shadowTexcoord, float3 shadowColor, float shadowBias)
{
    float depth = tex.Sample(samplerState, shadowTexcoord.xy).r;
    float s = step(shadowTexcoord.z - depth, shadowBias);
    return lerp(shadowColor, 1, s);
}

// ────────────────────────────────────────────────────────────────────────────
//  PCF フィルター付きソフトシャドウ
// ────────────────────────────────────────────────────────────────────────────
float3 CalcShadowColorPCFFilter(Texture2D tex, SamplerState samplerState,
                                float3 shadowTexcoord, float3 shadowColor,
                                float shadowBias, int PCFKernelSize)
{
    float2 texelSize;
    {
        uint width, height;
        tex.GetDimensions(width, height);
        texelSize = float2(1.0f / width, 1.0f / height);
    }

    float factor = 0;
    for (int x = -PCFKernelSize / 2; x <= PCFKernelSize / 2; ++x)
    {
        for (int y = -PCFKernelSize / 2; y <= PCFKernelSize / 2; ++y)
        {
            float depth = tex.Sample(samplerState,
                shadowTexcoord.xy + texelSize * float2(x, y)).r;
            factor += step(shadowTexcoord.z - depth, shadowBias);
        }
    }
    return lerp(shadowColor, 1, factor / (PCFKernelSize * PCFKernelSize));
}
