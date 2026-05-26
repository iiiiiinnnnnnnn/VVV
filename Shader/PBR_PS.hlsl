// PBR_PS.hlsl

#include "PBR.hlsli"

Texture2D DiffuseMap : register(t0);
SamplerState LinearSampler : register(s0);

Texture2D shadow_map : register(t10);
SamplerState shadow_sampler_state : register(s10);

float3 DirectBRDF(float3 diffuse_reflectance, float3 F0, float3 N, float3 V, float3 L, float3 LC, float roughness)
{
    float3 H = normalize(V + L);
    float NdotV = abs(dot(N, V)) + 0.001;
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float LdotH = saturate(dot(L, H));

    float D = NdotH * NdotH * (roughness * roughness - 1.0) + 1.0;
    D = (roughness * roughness) / (3.14159 * D * D);

    float F = F0 + (1.0 - F0) * pow(1.0 - LdotH, 5.0);

    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G = NdotL / (NdotL * (1.0 - k) + k);
    G *= NdotV / (NdotV * (1.0 - k) + k);

    float3 diffuse = diffuse_reflectance / 3.14159;
    float3 specular = (D * F * G) / (4.0 * NdotV * NdotL + 0.001);

    return (diffuse + specular) * LC * NdotL;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = DiffuseMap.Sample(LinearSampler, pin.texcoord) * materialColor;

    float3 N = normalize(pin.normal);
    float3 V = normalize(pin.position - viewPosition);
    float3 L = normalize(-lightManager.directionalLight.direction.xyz);
    float3 LC = lightManager.directionalLight.color.rgb * lightManager.directionalLight.color.a;

    float metalness = materialColor.a;
    float roughnessValue = clamp(adjust_roughness, 0.0001, 1.0);
    metalness = clamp(adjust_metalness, 0.0001, 1.0);

	// 拡散反射
    float3 diffuse_reflectance = lerp(color.rgb, 0.0f, metalness);
    float3 F0 = lerp(0.04f, color.rgb, metalness);

	// PBR計算
    float3 radiance = DirectBRDF(diffuse_reflectance, F0, N, V, L, LC, roughnessValue);

	// シャドウマップチェック
    float depth = shadow_map.Sample(shadow_sampler_state, pin.shadow_texcoord.xy).r;
    if (pin.shadow_texcoord.z - depth > shadow_bias)
    {
        radiance *= shadow_attenuation;
    }

    float3 total_color = radiance;

	// 点光源
    for (uint i = 0; i < lightManager.pointLightCount && i < 8; ++i)
    {
        float3 pointL = lightManager.pointLights[i].position - pin.position;
        float len = length(pointL);
        if (len < lightManager.pointLights[i].range)
        {
            float attenuation = 1.0 - (len / lightManager.pointLights[i].range);
            attenuation *= attenuation;
            pointL = normalize(pointL);
            float3 pointLC = lightManager.pointLights[i].color.rgb * lightManager.pointLights[i].color.a * attenuation;
            total_color += DirectBRDF(diffuse_reflectance, F0, N, V, pointL, pointLC, roughnessValue);
        }
    }

	// スポットライト
    for (uint j = 0; j < lightManager.spotLightCount && j < 8; ++j)
    {
        float3 spotL = lightManager.spotLights[j].position - pin.position;
        float len = length(spotL);
        if (len < lightManager.spotLights[j].range)
        {
            float attenuation = 1.0 - (len / lightManager.spotLights[j].range);
            attenuation *= attenuation;
            spotL = normalize(spotL);

            float3 spotDir = normalize(lightManager.spotLights[j].direction);
            float angle = dot(spotDir, -spotL);
            float area = lightManager.spotLights[j].innerConeAngle - lightManager.spotLights[j].outerConeAngle;
            attenuation *= saturate(1.0 - (lightManager.spotLights[j].innerConeAngle - angle) / area);

            float3 spotLC = lightManager.spotLights[j].color.rgb * lightManager.spotLights[j].color.a * attenuation;
            total_color += DirectBRDF(diffuse_reflectance, F0, N, V, spotL, spotLC, roughnessValue);
        }
    }

    color.rgb = total_color;
    return color;
}