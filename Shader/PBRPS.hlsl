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
Texture2D shadowMap : register(t8);
SamplerState shadowSampler : register(s1);

// IBLテクスチャ (slot 17〜19)
Texture2D lut_ggx : register(t17); // GGXルックアップテーブル
TextureCube specular_pmrem : register(t18); // 事前計算鏡面反射キューブマップ
TextureCube diffuse_iem : register(t19); // 事前計算拡散反射キューブマップ

float4 main(VS_OUT pin) : SV_TARGET
{
    // -------------------------------------------------------------------------
    // テクスチャサンプリング
    // albedo と emissive は sRGB テクスチャ → PBR計算はリニア空間で行うため変換
    // metalness/roughness/AO はリニアデータなのでそのまま使う
    // -------------------------------------------------------------------------
    float4 albedoSRGB = baseMap.Sample(linearSampler, pin.texcoord);
    float4 albedo = float4(pow(albedoSRGB.rgb, GammaFactor), albedoSRGB.a) * baseColor;

    float2 mrSample = metalnessRoughnessMap.Sample(linearSampler, pin.texcoord).gb;
    float ao = occlusionMap.Sample(linearSampler, pin.texcoord).r;

    float3 emissiveSRGB = emissiveMap.Sample(linearSampler, pin.texcoord).rgb;
    float3 emissive = pow(emissiveSRGB, GammaFactor) * emissiveColor.rgb;
    
    float finalRoughness = clamp(roughness, 0.0001f, 1.0f);
    float finalMetalness = clamp(metalness, 0.0001f, 1.0f);
    if (hasMetalRoughTexture)
    {
        finalRoughness = clamp(roughness * mrSample.x, 0.0001f, 1.0f);
        finalMetalness = clamp(metalness * mrSample.y, 0.0001f, 1.0f);
    }

    // PBRパラメーター
    float3 diffuse_reflectance = lerp(albedo.rgb, 0.0f, finalMetalness);
    float3 F0 = lerp(0.04f, albedo.rgb, finalMetalness);

    float3 N = normalize(pin.normal);
    float3 V = normalize(viewPosition - pin.position);

    float3 totalDiffuse = 0;
    float3 totalSpecular = 0;

    // ディレクショナルライト
    {
        float3 L = normalize(-lightManager.directionalLight.direction);
        float3 LC = lightManager.directionalLight.color.rgb * lightManager.directionalLight.color.a;
        float3 d, s;
        DirectBRDF(diffuse_reflectance, F0, N, V, L, LC, finalRoughness, d, s);
        totalDiffuse += d;
        totalSpecular += s;
    }

    // ポイントライト
    for (uint i = 0; i < lightManager.pointLightCount && i < MaxPointLights; ++i)
    {
        float3 toLight = lightManager.pointLights[i].position - pin.position;
        float len = length(toLight);
        if (len < lightManager.pointLights[i].range)
        {
            float atten = 1.0f - (len / lightManager.pointLights[i].range);
            atten *= atten;
            float3 L = normalize(toLight);
            float3 LC = lightManager.pointLights[i].color.rgb
                      * lightManager.pointLights[i].color.a * atten;
            float3 d, s;
            DirectBRDF(diffuse_reflectance, F0, N, V, L, LC, finalRoughness, d, s);
            totalDiffuse += d;
            totalSpecular += s;
        }
    }

    // スポットライト
    for (uint j = 0; j < lightManager.spotLightCount && j < MaxSpotLights; ++j)
    {
        float3 toLight = lightManager.spotLights[j].position - pin.position;
        float len = length(toLight);
        if (len < lightManager.spotLights[j].range)
        {
            float atten = 1.0f - (len / lightManager.spotLights[j].range);
            atten *= atten;
            float3 L = normalize(toLight);
            float3 spotDir = normalize(lightManager.spotLights[j].direction);
            float angle = dot(spotDir, -L);
            float area = lightManager.spotLights[j].innerConeAngle
                            - lightManager.spotLights[j].outerConeAngle;
            atten *= saturate(1.0f - (lightManager.spotLights[j].innerConeAngle - angle) / area);
            float3 LC = lightManager.spotLights[j].color.rgb
                      * lightManager.spotLights[j].color.a * atten;
            float3 d, s;
            DirectBRDF(diffuse_reflectance, F0, N, V, L, LC, finalRoughness, d, s);
            totalDiffuse += d;
            totalSpecular += s;
        }
    }
    
    // エリアライト（矩形面光源の近似：最近傍点法）
    for (uint k = 0; k < lightManager.areaLightCount && k < MaxAreaLights; ++k)
    {
        float3 up = normalize(lightManager.areaLights[k].direction);
        float3 right = normalize(lightManager.areaLights[k].right);
        float3 toSurf = pin.position - lightManager.areaLights[k].position;

        // 面上の最近傍点へのベクトルを求める
        float2 proj = float2(dot(toSurf, right), dot(toSurf, up));
        float hw = lightManager.areaLights[k].width * 0.5f;
        float hh = lightManager.areaLights[k].height * 0.5f;
        float2 clamped = float2(clamp(proj.x, -hw, hw), clamp(proj.y, -hh, hh));
        float3 nearest = lightManager.areaLights[k].position
                   + right * clamped.x + up * clamped.y;

        float3 toLight = nearest - pin.position;
        float len = length(toLight);
        if (len < lightManager.areaLights[k].range)
        {
            float atten = 1.0f - (len / lightManager.areaLights[k].range);
            atten *= atten;
            // 面の向きと入射角による減衰
            atten *= saturate(dot(normalize(-toSurf), up));
            float3 L = normalize(toLight);
            float3 LC = lightManager.areaLights[k].color.rgb
                  * lightManager.areaLights[k].color.a * atten;
            float3 d, s;
            DirectBRDF(diffuse_reflectance, F0, N, V, L, LC, finalRoughness, d, s);
            totalDiffuse += d;
            totalSpecular += s;
        }
    }
    
    float4 sc = shadowColor;
    if (isFace)
    {
        sc = float4(1.0f, 1.0f, 1.0f, 1.0f); // 表面は影なし（両面描画の裏面対策）
    }

    // シャドウ（PCFソフトシャドウ）
    float3 shadow = CalcShadowColorPCFFilter(
        shadowMap, shadowSampler,
        pin.shadowTexcoord,
        sc.rgb,
        shadowBias,
        pcfKernelSize);

    // IBL（間接光）
    float iblIntensity = lightManager.ambientColor.a;
    float3 iblDiffuse = DiffuseIBL(N, V, finalRoughness, diffuse_reflectance, F0,
                                     diffuse_iem, linearSampler) * iblIntensity;
    float3 iblSpecular = SpecularIBL(N, V, finalRoughness, F0,
                                     lut_ggx, specular_pmrem, linearSampler) * iblIntensity;

    // AO適用
    iblDiffuse = lerp(iblDiffuse, iblDiffuse * ao, occlusionStrength);
    iblSpecular = lerp(iblSpecular, iblSpecular * ao, occlusionStrength);

    // 合成（リニア空間）
    float3 color = (totalDiffuse + totalSpecular) * shadow
                 + iblDiffuse + iblSpecular
                 + emissive;

    // リニア → sRGB (ガンマ補正)
    //color = pow(max(color, 0.0f), 1.0f / GammaFactor);

    return float4(color, albedo.a);
}
