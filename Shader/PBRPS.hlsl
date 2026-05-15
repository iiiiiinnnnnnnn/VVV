// PBRPS.hlsl
// VVV 用 PBR ピクセルシェーダー
// Ex04_Base から移植 (2026-05-15)
// IBL テクスチャパス:
//   Data/Image/specular_pmrem.dds → t17
//   Data/Image/diffuse_iem.dds    → t18
//   Data/Image/lut_ggx.dds        → t19

#include "PBR.hlsli"

// ────────────────────────────────────────────────────────────────────────────
//  テクスチャ / サンプラー
// ────────────────────────────────────────────────────────────────────────────
Texture2D   albedoMap               : register(t0);
Texture2D   normalMap               : register(t1);
Texture2D   metalnessRoughnessMap   : register(t2); // VVV: G=roughness, B=metalness
Texture2D   occlusionMap            : register(t3);
Texture2D   shadowMap               : register(t8);
TextureCube specularPMREM           : register(t17); // specular_pmrem.dds
TextureCube diffuseIEM              : register(t18); // diffuse_iem.dds
Texture2D   lutGGX                  : register(t19); // lut_ggx.dds

SamplerState baseMapSampler         : register(s0);
SamplerState shadowMapSampler       : register(s1);

// ────────────────────────────────────────────────────────────────────────────
//  定数
// ────────────────────────────────────────────────────────────────────────────
static const float GammaFactor = 2.2f;

// ────────────────────────────────────────────────────────────────────────────
//  メイン
// ────────────────────────────────────────────────────────────────────────────
float4 main(VS_OUT pin) : SV_TARGET
{
    // ─── ベースカラー ───────────────────────────────────────────────────────
    float4 albedo = materialColor;
    {
        float4 sampled = albedoMap.Sample(baseMapSampler, pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GammaFactor); // リニア空間へ
        albedo *= sampled;
    }

    // ─── 法線マッピング ──────────────────────────────────────────────────────
    // VVV: tangent は TANGENT(float4) だが VS_OUT には float3 で格納済み
    float3 N = normalMap.Sample(baseMapSampler, pin.texcoord).rgb;
    // binormal を外積で算出
    float3 T = normalize(pin.tangent);
    float3 Nv = normalize(pin.normal);
    float3 B = normalize(cross(Nv, T));
    float3x3 TBN = { T, B, Nv };
    N = normalize(mul(N * 2.0f - 1.0f, TBN));

    // ─── メタルネス / ラフネス ────────────────────────────────────────────────
    float roughness, metalness;
    {
        float4 sampled = metalnessRoughnessMap.Sample(baseMapSampler, pin.texcoord);
        roughness = sampled.g;  // glTF 準拠: G = roughness
        metalness = sampled.b;  // glTF 準拠: B = metalness
    }

    // ─── オクルージョン ──────────────────────────────────────────────────────
    float occlusionFactor  = 1.0f;
    const float occlusionStrength = 1.0f;
    {
        occlusionFactor = occlusionMap.Sample(baseMapSampler, pin.texcoord).r;
    }

    // ─── PBR パラメーター ────────────────────────────────────────────────────
    float3 diffuseReflectance = lerp(albedo.rgb, 0.0f, metalness);
    float3 F0                 = lerp(0.04f, albedo.rgb, metalness);

    // 視線ベクトル (ピクセルから視点へ)
    float3 V = -normalize(cameraPosition.xyz - pin.position.xyz);

    // ─── 直接光ライティング ──────────────────────────────────────────────────
    float3 totalDiffuse  = (float3)0;
    float3 totalSpecular = (float3)0;

    // 平行光源
    {
        float3 diffuse = (float3)0, specular = (float3)0;
        float3 L  = normalize(lightDirection.xyz);
        float3 LC = lightColor.rgb;
        DirectBRDF(diffuseReflectance, F0, N, V, L, LC, roughness, diffuse, specular);

        // PCF シャドウ
        float3 shadow = CalcShadowColorPCFFilter(
            shadowMap, shadowMapSampler,
            pin.shadowTexcoord, shadowColor, shadowBias, PCFKernelSize);
        totalDiffuse  += diffuse  * shadow;
        totalSpecular += specular * shadow;
    }

    // ─── IBL ────────────────────────────────────────────────────────────────
    {
        // VVV の CbScene には ambientLightColor がないため、定数 1.0 を使用。
        // 必要なら PBRShader::CbScene に追加してください。
        float3 ambient = float3(1.0f, 1.0f, 1.0f);

        totalDiffuse  += ambient * DiffuseIBL(
            N, V, roughness, diffuseReflectance, F0,
            diffuseIEM, baseMapSampler);

        totalSpecular += ambient * SpecularIBL(
            N, V, roughness, F0,
            lutGGX, specularPMREM, baseMapSampler);
    }

    // ─── オクルージョン ──────────────────────────────────────────────────────
    totalDiffuse  = lerp(totalDiffuse,  totalDiffuse  * occlusionFactor, occlusionStrength);
    totalSpecular = lerp(totalSpecular, totalSpecular * occlusionFactor, occlusionStrength);

    // ─── 出力 ────────────────────────────────────────────────────────────────
    float3 color = totalDiffuse + totalSpecular;
    color = pow(color, 1.0f / GammaFactor); // sRGB 空間へ
    return float4(color, albedo.a);
}
