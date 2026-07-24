// PBRPS.hlsl

#include "PBR.hlsli"
#include "PBRFunctions.hlsli"
#include "ShadowmapFunctions.hlsli"

// マテリアルテクスチャ (slot 0〜4)
Texture2D baseMap : register(t0); // アルベド (sRGB)
Texture2D normalMap : register(t1); // 法線マップ (linear)
Texture2D metalnessRoughnessMap : register(t2); // G=roughness, B=metalness (linear)
Texture2D occlusionMap : register(t3); // AO (linear)
Texture2D emissiveMap : register(t4); // エミッシブ (sRGB)

SamplerState linearSampler : register(s0);

// シャドウマップ (slot 8)
Texture2D shadowMaps[ShadowCascadeCount] : register(t8);
SamplerState shadowSampler : register(s1);

// IBLテクスチャ (slot 17〜19)
Texture2D lut_ggx : register(t17); // GGXルックアップテーブル
TextureCube specular_pmrem : register(t18); // 事前計算鏡面反射キューブマップ
TextureCube diffuse_iem : register(t19); // 事前計算拡散反射キューブマップ

// -----------------------------------------------------------------------------
// 影の付きやすさを調整できる直接光BRDF
//
// shadowStrength:
//   1.0 = 通常のトゥーン影
//   0.0 = 影がかなり付きにくい、肌向け
//
// 重要:
//   完全にNdotLを消すと顔だけ白く浮くので、
//   shadowStrengthが低い時は「柔らかい肌用陰影」に寄せる。
// -----------------------------------------------------------------------------
void DirectBRDFShadowStrength(
    float3 diffuse_reflectance,
    float3 F0,
    float3 normal,
    float3 eye_vector,
    float3 light_vector,
    float3 light_color,
    float roughness,
    float shadow_strength,
    out float3 out_diffuse,
    out float3 out_specular)
{
    float3 N = normal;
    float3 L = light_vector;
    float3 V = eye_vector;
    float3 H = normalize(L + V);

    float rawNdotL = dot(N, L);

    float NdotV = max(0.0001f, dot(N, V));
    float NdotL = max(0.0001f, rawNdotL);
    float NdotH = max(0.0001f, dot(N, H));
    float VdotH = max(0.0001f, dot(V, H));

    float s = saturate(shadow_strength);

    // 通常のトゥーン影
    float hardThreshold = step(0.25f, NdotL);
    float hardToonFactor = lerp(0.2f, 1.0f, hardThreshold);

    // 肌用の柔らかい陰影
    // 鼻や頬の変な黒い段差を減らす。
    // ただし完全に1.0にはしないので、顔だけ白く浮きにくい。
    float softLight = smoothstep(-0.30f, 0.45f, rawNdotL);
    float softToonFactor = lerp(0.60f, 1.0f, softLight);

    // shadowStrengthが低いほど肌用の柔らかい陰影に近づく
    float toonFactor = lerp(softToonFactor, hardToonFactor, s);

    float3 irradiance = light_color * NdotL;

    out_diffuse =
        DiffuseBRDF(VdotH, F0, diffuse_reflectance)
        * light_color
        * toonFactor;

    out_specular =
        SpecularBRDF(NdotV, NdotL, NdotH, VdotH, F0, roughness)
        * irradiance;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    // -------------------------------------------------------------------------
    // テクスチャサンプリング
    // albedo と emissive は sRGB テクスチャ → PBR計算はリニア空間で行うため変換
    // metalness/roughness/AO はリニアデータなのでそのまま使う
    // -------------------------------------------------------------------------
    float4 albedoSRGB = useBaseColorTexture != 0
        ? baseMap.Sample(linearSampler, pin.texcoord)
        : float4(1.0, 1.0, 1.0, 1.0);

    float4 albedo =
    float4(
        pow(albedoSRGB.rgb, GammaFactor),
        albedoSRGB.a)
    * baseColor;

    float3 emissive;
    if (useEmissiveTexture != 0)
    {
        float3 emissiveSRGB =
        emissiveMap.Sample(linearSampler, pin.texcoord).rgb;
        float3 emissiveMask = pow(emissiveSRGB, GammaFactor);
        float emissiveStrength = max(emissiveMask.r, max(emissiveMask.g, emissiveMask.b));
        emissive = emissiveColor.rgb * emissiveColor.a * emissiveStrength;
    }
    else
    {
        emissive = emissiveColor.rgb * emissiveColor.a;
    }
    emissive += emissionColor.rgb * emissionColor.a;

    float finalMetalness = clamp(metalness, 0.0f, 1.0f);
    float finalRoughness = clamp(roughness, 0.0001f, 1.0f);

    if (useMetalnessTexture != 0 || useRoughnessTexture != 0)
    {
        // glTF系は G = roughness, B = metalness
        float2 mrSample = metalnessRoughnessMap.Sample(linearSampler, pin.texcoord).gb;

        if (useRoughnessTexture != 0)
        {
            finalRoughness = clamp(mrSample.x, 0.0001f, 1.0f);
        }

        if (useMetalnessTexture != 0)
        {
            finalMetalness = clamp(mrSample.y, 0.0f, 1.0f);
        }
    }

    float finalAO = clamp(occlusion, 0.0f, 1.0f);

	if (useOcclusionTexture != 0)
	{
		finalAO = occlusionMap.Sample(linearSampler, pin.texcoord).r * occlusion;
	}

    finalAO = lerp(1.0f, finalAO, occlusionStrength);

    // PBRパラメーター
    float3 diffuse_reflectance = lerp(albedo.rgb, 0.0f, finalMetalness);
    float3 F0 = lerp(0.04f, albedo.rgb, finalMetalness);

    float3 N = normalize(pin.normal);
    float3 V = normalize(viewPosition - pin.position);

    float fresnel = pow(saturate(1.0f - dot(N, V)), max(fresnelPower, 0.0001f));
    float3 rimEmission = fresnelColor.rgb * fresnelColor.a * fresnel * fresnelStrength;

    float3 totalDiffuse = 0.0f;
    float3 totalSpecular = 0.0f;

    // ディレクショナルライト
    {
        float3 L = normalize(-lightData.directionalLight.direction);
        float3 LC = lightData.directionalLight.color.rgb * lightData.directionalLight.color.a;

        float3 d, s;
        DirectBRDFShadowStrength(
            diffuse_reflectance,
            F0,
            N,
            V,
            L,
            LC,
            finalRoughness,
            shadowStrength,
            d,
            s);

        totalDiffuse += d;
        totalSpecular += s;
    }

    // ポイントライト
    for (uint i = 0; i < lightData.pointLightCount && i < MaxPointLights; ++i)
    {
        float3 toLight = lightData.pointLights[i].position - pin.position;
        float len = length(toLight);

        if (len < lightData.pointLights[i].range)
        {
            float atten = 1.0f - (len / lightData.pointLights[i].range);
            atten *= atten;

            float3 L = normalize(toLight);
            float3 LC =
                lightData.pointLights[i].color.rgb
                * lightData.pointLights[i].color.a
                * atten;

            float3 d, s;
            DirectBRDFShadowStrength(
                diffuse_reflectance,
                F0,
                N,
                V,
                L,
                LC,
                finalRoughness,
                shadowStrength,
                d,
                s);

            totalDiffuse += d;
            totalSpecular += s;
        }
    }

    // スポットライト
    for (uint j = 0; j < lightData.spotLightCount && j < MaxSpotLights; ++j)
    {
        float3 toLight = lightData.spotLights[j].position - pin.position;
        float len = length(toLight);

        if (len < lightData.spotLights[j].range)
        {
            float atten = 1.0f - (len / lightData.spotLights[j].range);
            atten *= atten;

            float3 L = normalize(toLight);
            float3 spotDir = normalize(lightData.spotLights[j].direction);
            float angle = dot(spotDir, -L);

            float area =
                lightData.spotLights[j].innerConeAngle
                - lightData.spotLights[j].outerConeAngle;

            atten *= saturate(
                1.0f
                - (lightData.spotLights[j].innerConeAngle - angle)
                / area);

            float3 LC =
                lightData.spotLights[j].color.rgb
                * lightData.spotLights[j].color.a
                * atten;

            float3 d, s;
            DirectBRDFShadowStrength(
                diffuse_reflectance,
                F0,
                N,
                V,
                L,
                LC,
                finalRoughness,
                shadowStrength,
                d,
                s);

            totalDiffuse += d;
            totalSpecular += s;
        }
    }

    // エリアライト（矩形面光源の近似：最近傍点法）
    for (uint k = 0; k < lightData.areaLightCount && k < MaxAreaLights; ++k)
    {
        float3 up = normalize(lightData.areaLights[k].direction);
        float3 right = normalize(lightData.areaLights[k].right);
        float3 toSurf = pin.position - lightData.areaLights[k].position;

        // 面上の最近傍点へのベクトルを求める
        float2 proj = float2(dot(toSurf, right), dot(toSurf, up));
        float hw = lightData.areaLights[k].width * 0.5f;
        float hh = lightData.areaLights[k].height * 0.5f;

        float2 clamped = float2(
            clamp(proj.x, -hw, hw),
            clamp(proj.y, -hh, hh));

        float3 nearest =
            lightData.areaLights[k].position
            + right * clamped.x
            + up * clamped.y;

        float3 toLight = nearest - pin.position;
        float len = length(toLight);

        if (len < lightData.areaLights[k].range)
        {
            float atten = 1.0f - (len / lightData.areaLights[k].range);
            atten *= atten;

            // 面の向きと入射角による減衰
            atten *= saturate(dot(normalize(-toSurf), up));

            float3 L = normalize(toLight);
            float3 LC =
                lightData.areaLights[k].color.rgb
                * lightData.areaLights[k].color.a
                * atten;

            float3 d, s;
            DirectBRDFShadowStrength(
                diffuse_reflectance,
                F0,
                N,
                V,
                L,
                LC,
                finalRoughness,
                shadowStrength,
                d,
                s);

            totalDiffuse += d;
            totalSpecular += s;
        }
    }

    float3 shadow = 1.0f.xxx;
    int cascadeIndex = SelectShadowCascade(
        dot(pin.position - viewPosition, camera_front.xyz),
        cascade_splits);
    if (cascadeIndex >= 0)
    {
        float3 shadowTexcoord = CalcShadowTexcoord(
            pin.position,
            light_view_projections[cascadeIndex]);
        if (IsShadowTexcoordValid(shadowTexcoord))
        {
            switch (cascadeIndex)
            {
            case 0:
                shadow = CalcShadowColorPCFFilter(
                    shadowMaps[0], shadowSampler, shadowTexcoord,
                    shadowColor.rgb, shadowBias, pcfKernelSize);
                break;
            case 1:
                shadow = CalcShadowColorPCFFilter(
                    shadowMaps[1], shadowSampler, shadowTexcoord,
                    shadowColor.rgb, shadowBias, pcfKernelSize);
                break;
            case 2:
                shadow = CalcShadowColorPCFFilter(
                    shadowMaps[2], shadowSampler, shadowTexcoord,
                    shadowColor.rgb, shadowBias, pcfKernelSize);
                break;
            default:
                shadow = CalcShadowColorPCFFilter(
                    shadowMaps[3], shadowSampler, shadowTexcoord,
                    shadowColor.rgb, shadowBias, pcfKernelSize);
                break;
            }
        }
    }

    // shadowStrengthが低いほど、シャドウマップの影も薄くする。
    // 「鼻などの変な影だけ薄くしたい、外部の影は残したい」場合は、
    // 下の1行をコメントアウトしてください。
    shadow = lerp(1.0f.xxx, shadow, saturate(shadowStrength));

    // IBL（間接光）
    float3 ambient =
    lightData.ambientColor.rgb
    * lightData.ambientColor.a;

    float3 iblDiffuse =
        DiffuseIBL(
            N,
            V,
            finalRoughness,
            diffuse_reflectance,
            F0,
            diffuse_iem,
            linearSampler)
        * ambient;

    float3 iblSpecular =
        SpecularIBL(
            N,
            V,
            finalRoughness,
            F0,
            lut_ggx,
            specular_pmrem,
            linearSampler)
        * ambient;

    // AO適用
    iblDiffuse *= finalAO;
    iblSpecular *= finalAO;

    // 合成（リニア空間）
    float3 color =
        (totalDiffuse + totalSpecular) * shadow
        + iblDiffuse
        + iblSpecular
        + emissive
        + rimEmission;

    color = ApplyDistanceFog(color, pin.position);

    return float4(color, albedo.a);
}
