// WaterSurfaceVS.hlsl
// Wave model adapted from tuxalin/water-shader (MIT License).

cbuffer WaterConstants : register(b0)
{
    row_major float4x4 viewProjection;
    row_major float4x4 world;
    row_major float4x4 inverseViewProjection;
    float3 cameraPosition;
    float time;
    float4 shallowColor;
    float4 deepColor;
    float waveScale;
    float waveSpeed;
    float waveStrength;
    float fresnelPower;
    float fresnelStrength;
    float opacity;
    float2 screenSize;
    float2 windDirection;
    float textureTiling;
    float normalIntensity;
    float3 lightDirection;
    float shininess;
    float4 lightColor;
    float4 ambientColor;
    float shoreFadeDistance;
    float3 shorePadding;
};

Texture2D heightTexture : register(t1);
SamplerState linearWrapSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 geometricNormal : TEXCOORD2;
    float waveCrest : TEXCOORD3;
};

void ApplyGerstnerWave(
    inout float3 position,
    inout float3 normal,
    float2 direction,
    float amplitude,
    float wavelength,
    float steepness,
    float timer)
{
    const float frequency = 6.2831853f / max(wavelength, 0.001f);
    const float phase = frequency * dot(position.xz, direction) + timer;
    const float cosine = cos(phase);
    const float sine = sin(phase);
    const float steepnessFactor = steepness / max(frequency * amplitude * 2.0f, 0.001f);
    const float waveAmplitude = frequency * amplitude;

    position += float3(
        steepnessFactor * amplitude * direction.x * cosine,
        amplitude * sine,
        steepnessFactor * amplitude * direction.y * cosine);
    normal += float3(
        -direction.x * waveAmplitude * cosine,
        1.0f - steepnessFactor * waveAmplitude * sine,
        -direction.y * waveAmplitude * cosine);
}

void ApplySineWave(
    inout float3 position,
    inout float3 normal,
    float2 direction,
    float amplitude,
    float wavelength,
    float timer)
{
    const float frequency = 6.2831853f / max(wavelength, 0.001f);
    const float phase = frequency * dot(position.xz, direction) + timer;
    position.y += sin(phase) * amplitude;
    normal += float3(
        -direction.x * frequency * amplitude * cos(phase),
        1.0f,
        -direction.y * frequency * amplitude * cos(phase));
}

VSOutput main(VSInput input)
{
    VSOutput output;
    float3 worldPosition = mul(float4(input.position, 1.0f), world).xyz;
    const float baseHeight = worldPosition.y;
    const float safeScale = max(waveScale, 0.01f);
    const float2 wind = normalize(windDirection);
    const float2 crossWind = normalize(float2(-wind.y, wind.x));
    // 洞窟湖向けに、流れよりもゆっくりした揺らぎとして動かす
    const float timer = time * waveSpeed * 0.4f;
    float3 geometricNormal = 0.0f;

    ApplyGerstnerWave(worldPosition, geometricNormal, wind,
        waveStrength * 0.42f, 8.0f / safeScale, 0.42f, timer * 1.05f);
    ApplyGerstnerWave(worldPosition, geometricNormal, normalize(wind + crossWind * 0.55f),
        waveStrength * 0.26f, 4.1f / safeScale, 0.32f, timer * 1.37f);
    ApplySineWave(worldPosition, geometricNormal, normalize(wind - crossWind * 0.8f),
        waveStrength * 0.17f, 2.2f / safeScale, timer * 1.83f);
    ApplySineWave(worldPosition, geometricNormal, crossWind,
        waveStrength * 0.09f, 1.2f / safeScale, -timer * 2.21f);

    const float2 textureUv = worldPosition.xz * 0.018f * textureTiling;
    const float2 scroll = wind * timer * 0.004f;
    float heightNoise = heightTexture.SampleLevel(
        linearWrapSampler, textureUv + scroll, 0.0f).r;
    heightNoise += heightTexture.SampleLevel(
        linearWrapSampler, textureUv * 1.9f - scroll * 1.4f, 0.0f).r * 0.5f;
    worldPosition.y += (heightNoise - 0.75f) * waveStrength * 0.12f;

    output.position = mul(float4(worldPosition, 1.0f), viewProjection);
    output.worldPosition = worldPosition;
    output.uv = input.uv;
    output.geometricNormal = normalize(geometricNormal);
    output.waveCrest = (worldPosition.y - baseHeight) / max(waveStrength, 0.001f);
    return output;
}
