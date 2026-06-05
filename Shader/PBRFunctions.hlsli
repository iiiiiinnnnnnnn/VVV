#include "ShadingFunctions.hlsli"

//--------------------------------------------
//	フレネル項
//--------------------------------------------
//F0	: 垂直入射時の反射率
//VdotH	: 視線ベクトルとハーフベクトル（光源へのベクトルと視点へのベクトルの中間ベクトル
float3 CalcFresnel(float3 F0, float VdotH)
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - VdotH, 0.0f, 1.0f), 5.0f);
}

//--------------------------------------------
//	拡散反射BRDF(正規化ランバートの拡散反射)
//--------------------------------------------
//VdotH		: 視線へのベクトルとハーフベクトルとの内積
//fresnelF0	: 垂直入射時のフレネル反射色
//diffuse_reflectance	: 入射光のうち拡散反射になる割合
float3 DiffuseBRDF(float VdotH, float3 fresnelF0, float3 diffuse_reflectance)
{
    return (1.0f - CalcFresnel(fresnelF0, VdotH)) * (diffuse_reflectance / PI);
}

//--------------------------------------------
//	法線分布関数
//--------------------------------------------
//NdotH		: 法線ベクトルとハーフベクトル（光源へのベクトルと視点へのベクトルの中間ベクトル）の内積
//roughness : 粗さ
float CalcNormalDistributionFunction(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float b = (NdotH * NdotH) * (a - 1.0f) + 1.0f;
    return a / (PI * b * b);
}

//--------------------------------------------
//	幾何減衰項の算出
//--------------------------------------------
//NdotL		: 法線ベクトルと光源へのベクトルとの内積
//NdotV		: 法線ベクトルと視線へのベクトルとの内積
//roughness : 粗さ
float CalcGeometryFunction(float NdotL, float NdotV, float roughness)
{
    float r = roughness * 0.5f;
    float shadowing = NdotL / (NdotL * (1.0 - r) + r);
    float masking = NdotV / (NdotV * (1.0 - r) + r);
    return shadowing * masking;
}

//--------------------------------------------
//	鏡面反射BRDF（クック・トランスのマイクロファセットモデル）
//--------------------------------------------
//NdotV		: 法線ベクトルと視線へのベクトルとの内積
//NdotL		: 法線ベクトルと光源へのベクトルとの内積
//NdotH		: 法線ベクトルとハーフベクトルとの内積
//VdotH		: 視線へのベクトルとハーフベクトルとの内積
//fresnelF0	: 垂直入射時のフレネル反射色
//roughness	: 粗さ
float3 SpecularBRDF(float NdotV, float NdotL, float NdotH, float VdotH, float3 fresnelF0, float roughness)
{
	//	D項(法線分布)
    float D = CalcNormalDistributionFunction(NdotH, roughness);
	//	G項(幾何減衰項)
    float G = CalcGeometryFunction(NdotL, NdotV, roughness);
	//	F項(フレネル反射)
    float3 F = CalcFresnel(fresnelF0, VdotH);

    //return D * G * F / (NdotL * NdotV * 4.0f);
    return D * G * F / (4.0f * NdotV);
}

//--------------------------------------------
//	直接光の物理ベースライティング
//--------------------------------------------
//diffuse_reflectance	: 入射光のうち拡散反射になる割合
//F0					: 垂直入射時のフレネル反射色
//normal				: 法線ベクトル(正規化済み)
//eye_vector			: 視点に向かうベクトル(正規化済み)
//light_vector			: 光源に向かうベクトル(正規化済み)
//light_color			: ライトカラー
//roughness				: 粗さ
void DirectBRDF(float3 diffuse_reflectance,
				float3 F0,
				float3 normal,
				float3 eye_vector,
				float3 light_vector,
				float3 light_color,
				float roughness,
				out float3 out_diffuse,
				out float3 out_specular)
{
    float3 N = normal;
    float3 L = light_vector;
    float3 V = eye_vector;
    float3 H = normalize(L + V);

    float NdotV = max(0.0001f, dot(N, V));
    float NdotL = max(0.0001f, dot(N, L));
    float NdotH = max(0.0001f, dot(N, H));
    float VdotH = max(0.0001f, dot(V, H));

    // ★なだらかなリアル光（NdotL）を、グラデーションのないパキッとした明暗（0.2 or 1.0）に変換
    float toonThreshold = step(0.25f, NdotL);
    float toonFactor = lerp(0.2f, 1.0f, toonThreshold);

    float3 irradiance = light_color * NdotL;

    // ★拡散反射（ベースの色）にだけ適用して、色ボケを防ぐ
    out_diffuse = DiffuseBRDF(VdotH, F0, diffuse_reflectance) * light_color * toonFactor;

    // ★鏡面反射（ツヤ）とリアルな質感はPBRのまま維持
    out_specular = SpecularBRDF(NdotV, NdotL, NdotH, VdotH, F0, roughness) * irradiance;
}

//--------------------------------------------
//	粗さを考慮したフレネル項の近似式
//--------------------------------------------
float3 CalcFresnelRoughness(float3 f0, float NdotV, float roughness)
{
    return f0 + (max((float3)(1.0f - roughness), f0) - f0) * pow(saturate(1.0f - NdotV), 5.0f);
}

//--------------------------------------------
//	拡散反射IBL
//--------------------------------------------
float3 DiffuseIBL(float3 normal, float3 eye_vector, float roughness,
                  float3 diffuse_reflectance, float3 f0,
                  TextureCube diffuse_iem_cube_map, SamplerState state)
{
    float3 N = normal;
    float3 V = eye_vector;
    float NdotV = max(0.0001f, dot(N, V));
    float3 kD = 1.0f - CalcFresnelRoughness(f0, NdotV, roughness);
    float3 irradiance = diffuse_iem_cube_map.Sample(state, normal).rgb;
    return diffuse_reflectance * irradiance * kD;
}

//--------------------------------------------
//	鏡面反射IBL
//--------------------------------------------
float3 SpecularIBL(float3 normal, float3 eye_vector, float roughness, float3 f0,
                   Texture2D lut_ggx_map, TextureCube specular_pmrem_cube_map, SamplerState state)
{
    float3 N = normal;
    float3 V = eye_vector;
    float NdotV = max(0.0001f, dot(N, V));
    float3 R = normalize(reflect(-V, N));

    uint width, height, mip_maps;
    specular_pmrem_cube_map.GetDimensions(0, width, height, mip_maps);
    float lod = roughness * float(mip_maps - 1);
    float3 specular_light = specular_pmrem_cube_map.SampleLevel(state, R, lod).rgb;

    float2 brdf_sample_point = saturate(float2(NdotV, roughness));
    float2 env_brdf = lut_ggx_map.Sample(state, brdf_sample_point).rg;

    return specular_light * (f0 * env_brdf.x + env_brdf.y);
}
