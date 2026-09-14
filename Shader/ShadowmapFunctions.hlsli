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

int SelectShadowCascade(float cameraDistance, float4 cascadeSplits)
{
    int cascadeIndex = -1;
    if (cameraDistance <= cascadeSplits.x) cascadeIndex = 0;
    else if (cameraDistance <= cascadeSplits.y) cascadeIndex = 1;
    else if (cameraDistance <= cascadeSplits.z) cascadeIndex = 2;
    else if (cameraDistance <= cascadeSplits.w) cascadeIndex = 3;
    return cascadeIndex;
}

// カスケード末端を次のマップへ徐々に混ぜ、解像度差が境界線になるのを修正
float CalcShadowCascadeBlend(float cameraDistance, int cascadeIndex, float4 cascadeSplits)
{
    float nearDistance = 0.0f;
    float farDistance = cascadeSplits.x;
    if (cascadeIndex == 1)
    {
        nearDistance = cascadeSplits.x;
        farDistance = cascadeSplits.y;
    }
    else if (cascadeIndex == 2)
    {
        nearDistance = cascadeSplits.y;
        farDistance = cascadeSplits.z;
    }
    float blendWidth = max((farDistance - nearDistance) * 0.15f, 0.001f);
    return saturate((cameraDistance - (farDistance - blendWidth)) / blendWidth);
}

bool IsShadowTexcoordValid(float3 shadowTexcoord)
{
    return all(shadowTexcoord >= 0.0f.xxx) &&
        all(shadowTexcoord <= 1.0f.xxx);
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
            float depth = tex.SampleLevel(
                samplerState,
                shadowTexcoord.xy + texelSize * float2(x, y),
                0.0f).r;
            factor += step(shadowTexcoord.z - depth, shadowBias);
        }
    }
    return lerp(shadowColor, 1, factor / (PCFKernelSize * PCFKernelSize));
}
