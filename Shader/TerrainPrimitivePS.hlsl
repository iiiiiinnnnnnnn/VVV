// TerrainPrimitivePS.hlsl

#include "TerrainPrimitive.hlsli"
#include "PBRFunctions.hlsli"
#include "ShadowmapFunctions.hlsli"

Texture2D shadowMaps[ShadowCascadeCount] : register(t8);

Texture2D lut_ggx : register(t17);
TextureCube specular_pmrem : register(t18);
TextureCube diffuse_iem : register(t19);

#define MAX_TERRAIN_LAYERS 16

cbuffer CbTerrainLayer : register(b4)
{
    int terrain_layer_count;
    int3 terrain_layer_dummy;
};

Texture2D<float4> terrainBaseTextures[MAX_TERRAIN_LAYERS] : register(t20);
Texture2D<float4> terrainNormalTextures[MAX_TERRAIN_LAYERS] : register(t36);

float3 UnpackNormalBC5(Texture2D<float4> tex, float2 uv)
{
    float2 xy = tex.Sample(linearSampler, uv).rg * 2.0f - 1.0f;
    float z = sqrt(saturate(1.0f - dot(xy, xy)));
    return float3(xy, z);
}

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);

    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float2 RotateUV(float2 uv, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(
        uv.x * c - uv.y * s,
        uv.x * s + uv.y * c);
}

float2 TerrainLayerUV(float2 baseUV, float2 worldXZ, float seed, float scaleMul)
{
    float2 cell = floor(worldXZ / 6.0f);
    float2 jitter = float2(
        Hash21(cell + seed),
        Hash21(cell + seed + 19.17f)) - 0.5f;

    float angle = (Hash21(cell + seed + 37.91f) - 0.5f) * 0.9f;
    return RotateUV(baseUV * scaleMul + jitter * 0.18f, angle);
}

float4 SampleAntiTile(Texture2D<float4> tex, float2 baseUV, float2 worldXZ, float seed)
{
    float n = ValueNoise(worldXZ * 0.045f + seed);
    float2 uvA = TerrainLayerUV(baseUV, worldXZ, seed, 1.0f);
    float2 uvB = TerrainLayerUV(baseUV, worldXZ, seed + 71.3f, 1.37f);
    return lerp(
        tex.Sample(linearSampler, uvA),
        tex.Sample(linearSampler, uvB),
        smoothstep(0.25f, 0.75f, n));
}

float3 SampleAntiTileNormal(Texture2D<float4> tex, float2 baseUV, float2 worldXZ, float seed)
{
    float n = ValueNoise(worldXZ * 0.045f + seed);
    float2 uvA = TerrainLayerUV(baseUV, worldXZ, seed, 1.0f);
    float2 uvB = TerrainLayerUV(baseUV, worldXZ, seed + 71.3f, 1.37f);

    float3 normalA = UnpackNormalBC5(tex, uvA);
    float3 normalB = UnpackNormalBC5(tex, uvB);
    return normalize(lerp(normalA, normalB, smoothstep(0.25f, 0.75f, n)));
}

float4 SampleTerrainBaseLayer(int layerIndex, float2 baseUV, float2 worldXZ)
{
    layerIndex = clamp(layerIndex, 0, max(terrain_layer_count - 1, 0));
    switch (layerIndex)
    {
    case 0: return SampleAntiTile(terrainBaseTextures[0], baseUV, worldXZ, 3.1f);
    case 1: return SampleAntiTile(terrainBaseTextures[1], baseUV, worldXZ, 16.47f);
    case 2: return SampleAntiTile(terrainBaseTextures[2], baseUV, worldXZ, 29.84f);
    case 3: return SampleAntiTile(terrainBaseTextures[3], baseUV, worldXZ, 43.21f);
    case 4: return SampleAntiTile(terrainBaseTextures[4], baseUV, worldXZ, 56.58f);
    case 5: return SampleAntiTile(terrainBaseTextures[5], baseUV, worldXZ, 69.95f);
    case 6: return SampleAntiTile(terrainBaseTextures[6], baseUV, worldXZ, 83.32f);
    case 7: return SampleAntiTile(terrainBaseTextures[7], baseUV, worldXZ, 96.69f);
    case 8: return SampleAntiTile(terrainBaseTextures[8], baseUV, worldXZ, 110.06f);
    case 9: return SampleAntiTile(terrainBaseTextures[9], baseUV, worldXZ, 123.43f);
    case 10: return SampleAntiTile(terrainBaseTextures[10], baseUV, worldXZ, 136.8f);
    case 11: return SampleAntiTile(terrainBaseTextures[11], baseUV, worldXZ, 150.17f);
    case 12: return SampleAntiTile(terrainBaseTextures[12], baseUV, worldXZ, 163.54f);
    case 13: return SampleAntiTile(terrainBaseTextures[13], baseUV, worldXZ, 176.91f);
    case 14: return SampleAntiTile(terrainBaseTextures[14], baseUV, worldXZ, 190.28f);
    default: return SampleAntiTile(terrainBaseTextures[15], baseUV, worldXZ, 203.65f);
    }
}

float3 SampleTerrainNormalLayer(int layerIndex, float2 baseUV, float2 worldXZ)
{
    layerIndex = clamp(layerIndex, 0, max(terrain_layer_count - 1, 0));
    switch (layerIndex)
    {
    case 0: return SampleAntiTileNormal(terrainNormalTextures[0], baseUV, worldXZ, 3.1f);
    case 1: return SampleAntiTileNormal(terrainNormalTextures[1], baseUV, worldXZ, 16.47f);
    case 2: return SampleAntiTileNormal(terrainNormalTextures[2], baseUV, worldXZ, 29.84f);
    case 3: return SampleAntiTileNormal(terrainNormalTextures[3], baseUV, worldXZ, 43.21f);
    case 4: return SampleAntiTileNormal(terrainNormalTextures[4], baseUV, worldXZ, 56.58f);
    case 5: return SampleAntiTileNormal(terrainNormalTextures[5], baseUV, worldXZ, 69.95f);
    case 6: return SampleAntiTileNormal(terrainNormalTextures[6], baseUV, worldXZ, 83.32f);
    case 7: return SampleAntiTileNormal(terrainNormalTextures[7], baseUV, worldXZ, 96.69f);
    case 8: return SampleAntiTileNormal(terrainNormalTextures[8], baseUV, worldXZ, 110.06f);
    case 9: return SampleAntiTileNormal(terrainNormalTextures[9], baseUV, worldXZ, 123.43f);
    case 10: return SampleAntiTileNormal(terrainNormalTextures[10], baseUV, worldXZ, 136.8f);
    case 11: return SampleAntiTileNormal(terrainNormalTextures[11], baseUV, worldXZ, 150.17f);
    case 12: return SampleAntiTileNormal(terrainNormalTextures[12], baseUV, worldXZ, 163.54f);
    case 13: return SampleAntiTileNormal(terrainNormalTextures[13], baseUV, worldXZ, 176.91f);
    case 14: return SampleAntiTileNormal(terrainNormalTextures[14], baseUV, worldXZ, 190.28f);
    default: return SampleAntiTileNormal(terrainNormalTextures[15], baseUV, worldXZ, 203.65f);
    }
}

// Terrain PBR FUnction
#define TERRAIN_PBR
#ifdef TERRAIN_PBR
void DirectBRDFShadowStrength(
    float3 diffuse_reflectance,
    float3 F0,
    float3 normal,
    float3 eye_vector,
    float3 light_vector,
    float3 light_color,
    float roughnessValue,
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

    float hardThreshold = step(0.25f, NdotL);
    float hardToonFactor = lerp(0.2f, 1.0f, hardThreshold);

    float softLight = smoothstep(-0.30f, 0.45f, rawNdotL);
    float softToonFactor = lerp(0.60f, 1.0f, softLight);

    float toonFactor = lerp(softToonFactor, hardToonFactor, s);
    float3 irradiance = light_color * NdotL;

    out_diffuse =
        DiffuseBRDF(VdotH, F0, diffuse_reflectance)
        * light_color
        * toonFactor;

    out_specular =
        SpecularBRDF(NdotV, NdotL, NdotH, VdotH, F0, roughnessValue)
        * irradiance;
}


float3 TerrainCalcShadowColorPCF(
    Texture2D tex,
    SamplerState samplerState,
    float3 shadowTexcoord,
    float3 shadowColor,
    float shadowBias,
    int kernelSize)
{
    uint width;
    uint height;
    tex.GetDimensions(width, height);

    float2 texelSize = 1.0f / float2(width, height);

    int k = clamp(kernelSize, 1, 9);
    if ((k % 2) == 0)
    {
        k += 1;
    }

    int halfKernel = k / 2;

    float factor = 0.0f;
    float count = 0.0f;

    [unroll]
    for (int x = -4; x <= 4; ++x)
    {
        [unroll]
        for (int y = -4; y <= 4; ++y)
        {
            if (abs(x) <= halfKernel && abs(y) <= halfKernel)
            {
                float depth = tex.SampleLevel(
                    samplerState,
                    shadowTexcoord.xy + texelSize * float2(x, y),
                    0.0f).r;

                factor += step(shadowTexcoord.z - depth, shadowBias);
                count += 1.0f;
            }
        }
    }

    return lerp(shadowColor, 1.0f.xxx, factor / max(count, 1.0f));
}
#endif

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 tilingCoord = pin.texcoord * tilling_scale;
    float2 worldXZ = pin.position.xz;
    float terrainDistanceFade = saturate(DistanceFogFactor(pin.position) / 0.82f);

    int layerCount = max(terrain_layer_count, 1);
    float layerRate = saturate(
        terrainDataMap.Sample(shadowSampler, pin.texcoord).g);
    float layerPosition = layerRate * (float)(layerCount - 1);
    int layer0 = (int)floor(layerPosition);
    int layer1 = min(layer0 + 1, layerCount - 1);
    float layerBlend = frac(layerPosition);

    float4 albedoSRGB = lerp(
        SampleTerrainBaseLayer(layer0, tilingCoord, worldXZ),
        SampleTerrainBaseLayer(layer1, tilingCoord, worldXZ),
        layerBlend);

    float4 albedo =
        float4(pow(albedoSRGB.rgb, GammaFactor), albedoSRGB.a)
        * baseColor;

    float albedoLuminance = dot(albedo.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    albedo.rgb = lerp(
        albedo.rgb,
        lerp(albedoLuminance.xxx, albedo.rgb, 0.55f),
        terrainDistanceFade * 0.45f);

    float3 emissive =
        emissiveColor.rgb * emissiveColor.a
        + emissionColor.rgb * emissionColor.a;

    float finalMetalness = clamp(metalness, 0.0f, 1.0f);
    float finalRoughness = clamp(roughness, 0.0001f, 1.0f);
    float finalAO = lerp(
        1.0f,
        clamp(occlusion, 0.0f, 1.0f),
        clamp(occlusionStrength, 0.0f, 1.0f));

    float3 diffuse_reflectance = lerp(
        albedo.rgb,
        0.0f,
        finalMetalness);

    float3 F0 = lerp(
        0.04f,
        albedo.rgb,
        finalMetalness);

    float3 normalTex = normalize(lerp(
        SampleTerrainNormalLayer(layer0, tilingCoord, worldXZ),
        SampleTerrainNormalLayer(layer1, tilingCoord, worldXZ),
        layerBlend));
    normalTex = normalize(lerp(normalTex, float3(0.0f, 0.0f, 1.0f), terrainDistanceFade * 0.65f));

    float3 baseN = normalize(pin.normal);

    float3 T = normalize(pin.tangent);
    T = normalize(T - baseN * dot(baseN, T));

    float3 B = normalize(cross(baseN, T));

    float3 N = normalize(
        normalTex.x * T +
        normalTex.y * B +
        normalTex.z * baseN
    );
    
    float3 V = normalize(viewPosition - pin.position);

    float3 totalDiffuse = 0.0f;
    float3 totalSpecular = 0.0f;

    {
        float3 L = normalize(-lightData.directionalLight.direction);
        float3 LC =
            lightData.directionalLight.color.rgb
            * lightData.directionalLight.color.a;

        float3 d;
        float3 s;

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

    for (uint i = 0; i < lightData.pointLightCount && i < MaxPointLights; ++i)
    {
        float3 toLight = lightData.pointLights[i].position - pin.position;
        float len = length(toLight);

        if (len < lightData.pointLights[i].range)
        {
            float atten = 1.0f - len / lightData.pointLights[i].range;
            atten *= atten;

            float3 L = normalize(toLight);
            float3 LC =
                lightData.pointLights[i].color.rgb
                * lightData.pointLights[i].color.a
                * atten;

            float3 d;
            float3 s;

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

    for (uint j = 0; j < lightData.spotLightCount && j < MaxSpotLights; ++j)
    {
        float3 toLight = lightData.spotLights[j].position - pin.position;
        float len = length(toLight);

        if (len < lightData.spotLights[j].range)
        {
            float atten = 1.0f - len / lightData.spotLights[j].range;
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

            float3 d;
            float3 s;

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

    for (uint k = 0; k < lightData.areaLightCount && k < MaxAreaLights; ++k)
    {
        float3 up = normalize(lightData.areaLights[k].direction);
        float3 right = normalize(lightData.areaLights[k].right);
        float3 toSurface = pin.position - lightData.areaLights[k].position;

        float2 projection = float2(
            dot(toSurface, right),
            dot(toSurface, up));

        float halfWidth = lightData.areaLights[k].width * 0.5f;
        float halfHeight = lightData.areaLights[k].height * 0.5f;

        float2 clampedProjection = float2(
            clamp(projection.x, -halfWidth, halfWidth),
            clamp(projection.y, -halfHeight, halfHeight));

        float3 nearest =
            lightData.areaLights[k].position
            + right * clampedProjection.x
            + up * clampedProjection.y;

        float3 toLight = nearest - pin.position;
        float len = length(toLight);

        if (len < lightData.areaLights[k].range)
        {
            float atten = 1.0f - len / lightData.areaLights[k].range;
            atten *= atten;
            atten *= saturate(dot(normalize(-toSurface), up));

            float3 L = normalize(toLight);
            float3 LC =
                lightData.areaLights[k].color.rgb
                * lightData.areaLights[k].color.a
                * atten;

            float3 d;
            float3 s;

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
                shadow = TerrainCalcShadowColorPCF(
                    shadowMaps[0], shadowSampler, shadowTexcoord,
                    shadowColor.rgb, shadowBias, pcfKernelSize);
                break;
            case 1:
                shadow = TerrainCalcShadowColorPCF(
                    shadowMaps[1], shadowSampler, shadowTexcoord,
                    shadowColor.rgb, shadowBias, pcfKernelSize);
                break;
            case 2:
                shadow = TerrainCalcShadowColorPCF(
                    shadowMaps[2], shadowSampler, shadowTexcoord,
                    shadowColor.rgb, shadowBias, pcfKernelSize);
                break;
            default:
                shadow = TerrainCalcShadowColorPCF(
                    shadowMaps[3], shadowSampler, shadowTexcoord,
                    shadowColor.rgb, shadowBias, pcfKernelSize);
                break;
            }
        }
    }

    shadow = lerp(
        1.0f.xxx,
        shadow,
        saturate(shadowStrength));

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

    iblDiffuse *= finalAO;
    iblSpecular *= finalAO;

    float ambientShadow = lerp(0.0f, 1.0f, saturate(shadowStrength));
    iblDiffuse *= lerp(1.0f.xxx, shadow, ambientShadow);
    iblSpecular *= lerp(1.0f.xxx, shadow, ambientShadow);

    float3 color =
        (totalDiffuse + totalSpecular) * shadow
        + iblDiffuse
        + iblSpecular
        + emissive;

    color = ApplyDistanceFog(color, pin.position);

    return float4(color, albedo.a);
}
