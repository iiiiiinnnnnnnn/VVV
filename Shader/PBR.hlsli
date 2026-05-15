// PBR.hlsli
// VVV用 PBR + ShadowMap ヘッダー
// Ex04_Base から移植 (2026-05-15)

#include "Scene.hlsli"
#include "Skinning.hlsli"
#include "ShadowmapFunctions.hlsli"

// ────────────────────────────────────────────────────────────────────────────
//  頂点シェーダー出力構造体
// ────────────────────────────────────────────────────────────────────────────
struct VS_OUT
{
    float4 vertex           : SV_POSITION;
    float3 position         : POSITION;     // ワールド座標
    float3 normal           : NORMAL;
    float3 tangent          : TANGENT;
    float2 texcoord         : TEXCOORD;
    float3 shadowTexcoord   : TEXCOORD1;    // シャドウマップ参照用UV + 深度
};

// ────────────────────────────────────────────────────────────────────────────
//  定数バッファ  (VVV の既存スロットを避けて配置)
//    b6 : CbSkeleton  (Skinning.hlsli / ModelRenderer)
//    b7 : CbScene     (Scene.hlsli    / ModelRenderer)
//    b8 : CbMesh      (マテリアルカラー)
//    b9 : CbShadowmap (シャドウマップ用)
// ────────────────────────────────────────────────────────────────────────────
cbuffer CbMesh : register(b8)
{
    float4 materialColor;
};

cbuffer CbShadowmap : register(b9)
{
    row_major float4x4 lightViewProjection; // ライトビュープロジェクション行列
    float3  shadowColor;                    // 影の色
    float   shadowBias;                     // 深度値比較時のオフセット
    int     PCFKernelSize;                  // ソフトシャドーのカーネルサイズ
    float3  shadowDummy;
};

// ────────────────────────────────────────────────────────────────────────────
//  PBR 計算関数
// ────────────────────────────────────────────────────────────────────────────
static const float PI = 3.1415926f;

// フレネル項
float3 CalcFresnel(float3 F0, float VdotH)
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - VdotH, 0.0f, 1.0f), 5.0f);
}

// 拡散反射 BRDF (正規化ランバート)
float3 DiffuseBRDF(float VdotH, float3 fresnelF0, float3 diffuse_reflectance)
{
    return (1.0f - CalcFresnel(fresnelF0, VdotH)) * (diffuse_reflectance / PI);
}

// 法線分布関数 (GGX)
float CalcNormalDistributionFunction(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float b = (NdotH * NdotH) * (a - 1.0f) + 1.0f;
    return a / (PI * b * b);
}

// 幾何減衰項
float CalcGeometryFunction(float NdotL, float NdotV, float roughness)
{
    float r = roughness * 0.5f;
    float shadowing = NdotL / (NdotL * (1.0f - r) + r);
    float masking   = NdotV / (NdotV * (1.0f - r) + r);
    return shadowing * masking;
}

// 鏡面反射 BRDF (Cook-Torrance)
float3 SpecularBRDF(float NdotV, float NdotL, float NdotH, float VdotH,
                    float3 fresnelF0, float roughness)
{
    float  D = CalcNormalDistributionFunction(NdotH, roughness);
    float  G = CalcGeometryFunction(NdotL, NdotV, roughness);
    float3 F = CalcFresnel(fresnelF0, VdotH);
    return D * G * F / (NdotL * NdotV * 4.0f);
}

// 直接光 PBR ライティング
void DirectBRDF(float3 diffuse_reflectance, float3 F0,
                float3 normal, float3 eye_vector, float3 light_vector,
                float3 light_color, float roughness,
                out float3 out_diffuse, out float3 out_specular)
{
    float3 N = normal;
    float3 L = -light_vector;
    float3 V = -eye_vector;
    float3 H = normalize(L + V);

    float NdotV = max(0.0001f, dot(N, V));
    float NdotL = max(0.0001f, dot(N, L));
    float NdotH = max(0.0001f, dot(N, H));
    float VdotH = max(0.0001f, dot(V, H));

    float3 irradiance = light_color * NdotL;
    out_diffuse  = DiffuseBRDF(VdotH, F0, diffuse_reflectance) * irradiance;
    out_specular = SpecularBRDF(NdotV, NdotL, NdotH, VdotH, F0, roughness) * irradiance;
}

// ────────────────────────────────────────────────────────────────────────────
//  IBL 計算関数
// ────────────────────────────────────────────────────────────────────────────

float4 SampleLutGGX(float2 brdf_sample_point, Texture2D lut_ggx_map, SamplerState state)
{
    return lut_ggx_map.Sample(state, brdf_sample_point);
}

float4 SampleDiffuseIEM(float3 v, TextureCube diffuse_iem_cube_map, SamplerState state)
{
    return diffuse_iem_cube_map.Sample(state, v);
}

float4 SampleSpecularPMREM(float3 v, float roughness,
                           TextureCube specular_pmrem_cube_map, SamplerState state)
{
    uint width, height, mip_maps;
    specular_pmrem_cube_map.GetDimensions(0, width, height, mip_maps);
    float lod = roughness * float(mip_maps - 1);
    return specular_pmrem_cube_map.SampleLevel(state, v, lod);
}

// 粗さを考慮したフレネル近似
float3 CalcFresnelRoughness(float3 f0, float NdotV, float roughness)
{
    return f0 + (max((float3)(1.0f - roughness), f0) - f0) * pow(saturate(1.0f - NdotV), 5.0f);
}

// 拡散反射 IBL
float3 DiffuseIBL(float3 normal, float3 eye_vector, float roughness,
                  float3 diffuse_reflectance, float3 f0,
                  TextureCube diffuse_iem_cube_map, SamplerState state)
{
    float3 N = normal;
    float3 V = -eye_vector;
    float NdotV = max(0.0001f, dot(N, V));
    float3 kD = 1.0f - CalcFresnelRoughness(f0, NdotV, roughness);
    float3 irradiance = SampleDiffuseIEM(normal, diffuse_iem_cube_map, state).rgb;
    return diffuse_reflectance * irradiance * kD;
}

// 鏡面反射 IBL
float3 SpecularIBL(float3 normal, float3 eye_vector, float roughness, float3 f0,
                   Texture2D lut_ggx_map,
                   TextureCube specular_pmrem_cube_map, SamplerState state)
{
    float3 N = normal;
    float3 V = -eye_vector;
    float NdotV = max(0.0001f, dot(N, V));
    float3 R = normalize(reflect(-V, N));
    float3 radiance = SampleSpecularPMREM(R, roughness, specular_pmrem_cube_map, state).rgb;
    float2 brdf_sample_point = saturate(float2(NdotV, roughness));
    float2 env_brdf = SampleLutGGX(brdf_sample_point, lut_ggx_map, state).rg;
    return radiance * (f0 * env_brdf.x + env_brdf.y);
}
