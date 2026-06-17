// TerrainPrimitivePS.hlsl

#include "TerrainPrimitive.hlsli"
#include "PBRFunctions.hlsli"

Texture2D shadowMap : register(t8);

Texture2D lut_ggx : register(t17);
TextureCube specular_pmrem : register(t18);
TextureCube diffuse_iem : register(t19);

Texture2D<float4> rockTexture : register(t20);
Texture2D<float4> rockTextureNormal : register(t21);
Texture2D<float4> dirtTexture : register(t22);
Texture2D<float4> dirtTextureNormal : register(t23);
Texture2D<float4> grassTexture : register(t24);
Texture2D<float4> grassTextureNormal : register(t25);

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

    float4 rockSRGB = SampleAntiTile(rockTexture, tilingCoord, worldXZ, 3.1f);
    float4 dirtSRGB = SampleAntiTile(dirtTexture, tilingCoord, worldXZ, 17.7f);
    float4 grassSRGB = SampleAntiTile(grassTexture, tilingCoord, worldXZ, 42.4f);

    float blendRate = saturate(
        terrainDataMap.Sample(shadowSampler, pin.texcoord).g);

    float4 albedoSRGB = lerp(
        rockSRGB,
        dirtSRGB,
        smoothstep(0.0f, 0.5f, blendRate));

    albedoSRGB = lerp(
        albedoSRGB,
        grassSRGB,
        smoothstep(0.5f, 1.0f, blendRate));

    float4 albedo =
        float4(pow(albedoSRGB.rgb, GammaFactor), albedoSRGB.a)
        * baseColor;

    float3 emissive = emissiveColor.rgb;

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

    float3 rockNormalTex = SampleAntiTileNormal(rockTextureNormal, tilingCoord, worldXZ, 3.1f);
    float3 dirtNormalTex = SampleAntiTileNormal(dirtTextureNormal, tilingCoord, worldXZ, 17.7f);
    float3 grassNormalTex = SampleAntiTileNormal(grassTextureNormal, tilingCoord, worldXZ, 42.4f);

    float dirtBlend = smoothstep(0.0f, 0.5f, blendRate);
    float grassBlend = smoothstep(0.5f, 1.0f, blendRate);

    float3 normalTex = normalize(lerp(rockNormalTex, dirtNormalTex, dirtBlend));
    normalTex = normalize(lerp(normalTex, grassNormalTex, grassBlend));

    float3 baseN = normalize(pin.normal);

    float3 T = normalize(float3(1, 0, 0));
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

    if (pin.shadowTexcoord.x >= 0.0f &&
        pin.shadowTexcoord.x <= 1.0f &&
        pin.shadowTexcoord.y >= 0.0f &&
        pin.shadowTexcoord.y <= 1.0f &&
        pin.shadowTexcoord.z >= 0.0f &&
        pin.shadowTexcoord.z <= 1.0f)
    {
        shadow = TerrainCalcShadowColorPCF(
            shadowMap,
            shadowSampler,
            pin.shadowTexcoord,
            shadowColor.rgb,
            shadowBias,
            pcfKernelSize);
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

    return float4(color, albedo.a);
}
