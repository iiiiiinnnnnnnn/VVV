// WaterSurfacePS.hlsl
// Multi-scale normals, Fresnel radiance and foam adapted from
// tuxalin/water-shader (MIT License).

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

Texture2D normalTexture : register(t0);
Texture2D heightTexture : register(t1);
Texture2D foamTexture : register(t2);
TextureCube environmentTexture : register(t3);
Texture2D sceneDepthTexture : register(t4);
SamplerState linearWrapSampler : register(s0);
SamplerState linearClampSampler : register(s1);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 geometricNormal : TEXCOORD2;
    float waveCrest : TEXCOORD3;
};

float3 UnpackWaterNormal(float2 uv)
{
    const float3 packed = normalTexture.Sample(linearWrapSampler, uv).xyz * 2.0f - 1.0f;
    return float3(packed.x, packed.z, packed.y);
}

float3 ComputeWaterNormal(float3 worldPosition, float3 geometricNormal)
{
    const float2 wind = normalize(windDirection);
    const float2 crossWind = float2(-wind.y, wind.x);
    const float2 timedWind = wind * time * waveSpeed * 0.35f;
    const float2 uv = worldPosition.xz * 0.05f * textureTiling;

    float3 wavesNormal = float3(0.0f, 1.0f, 0.0f);
    wavesNormal += UnpackWaterNormal(uv * 1.6f + timedWind * 0.025f) * 0.42f;
    wavesNormal += UnpackWaterNormal(uv * 0.8f - timedWind * 0.017f) * 0.34f;
    wavesNormal += UnpackWaterNormal(
        uv * 0.5f + crossWind * time * waveSpeed * 0.010f) * 0.20f;
    wavesNormal += UnpackWaterNormal(
        uv * 0.3f - crossWind * time * waveSpeed * 0.006f) * 0.14f;
    wavesNormal = normalize(wavesNormal);

    return normalize(lerp(geometricNormal, wavesNormal, saturate(normalIntensity)));
}

float3 ReconstructWorldPosition(float2 screenUv, float depth)
{
    const float4 clipPosition = float4(
        screenUv.x * 2.0f - 1.0f,
        1.0f - screenUv.y * 2.0f,
        depth,
        1.0f);
    float4 worldPosition = mul(clipPosition, inverseViewProjection);
    worldPosition.xyz /= max(worldPosition.w, 0.00001f);
    return worldPosition.xyz;
}

float4 main(PSInput input) : SV_TARGET
{
	const float2 screenUv = input.position.xy / max(screenSize, 1.0f);
	const float sceneDepth = sceneDepthTexture.Sample(
		linearClampSampler, screenUv).r;
	const float3 terrainPosition = ReconstructWorldPosition(screenUv, sceneDepth);
	const float waterDepth = max(input.worldPosition.y - terrainPosition.y, 0.0f);
	const float shorelineWidth = max(shoreFadeDistance, 0.01f) + waveStrength * 0.2f;
	const float shorelineAntialias = max(fwidth(waterDepth) * 1.5f, 0.035f);
	const float shorelineFade = smoothstep(
		0.0f, shorelineWidth + shorelineAntialias, waterDepth);

    const float3 normal = ComputeWaterNormal(
        input.worldPosition, normalize(input.geometricNormal));
    const float3 viewDirection = normalize(cameraPosition - input.worldPosition);
    const float3 toLight = normalize(-lightDirection);

    const float facingRate = saturate(dot(normal, viewDirection));
    const float fresnel = pow(
        1.0f - facingRate, max(fresnelPower, 0.1f));
    const float diffuse = saturate(dot(normal, toLight));
    const float3 halfVector = normalize(viewDirection + toLight);
    const float specular = pow(
        saturate(dot(normal, halfVector)), max(shininess, 1.0f));

    const float ripple = saturate(0.5f + input.waveCrest * 0.28f);
    float3 waterColor = lerp(deepColor.rgb, shallowColor.rgb, ripple);
    const float3 directionalRadiance = lightColor.rgb * lightColor.a;
    waterColor *= ambientColor.rgb + directionalRadiance * diffuse * 0.34f;

    const float3 reflectionDirection = reflect(-viewDirection, normal);
    const float3 reflectionColor = environmentTexture.SampleLevel(
        linearWrapSampler, reflectionDirection, 2.0f).rgb;
    waterColor = lerp(
        waterColor, reflectionColor,
        saturate(fresnel * fresnelStrength));
    waterColor += directionalRadiance * specular * (0.18f + fresnel * 0.82f);

    const float2 foamUv = input.worldPosition.xz * 0.035f * textureTiling;
    const float2 foamScroll = normalize(windDirection) * time * waveSpeed * 0.008f;
    const float foamNoiseA = foamTexture.Sample(
        linearWrapSampler, foamUv + foamScroll).r;
    const float foamNoiseB = foamTexture.Sample(
        linearWrapSampler, foamUv * -0.57f - foamScroll * 0.73f).r;
    const float crestAmount = saturate(input.waveCrest * 0.5f + 0.5f);
    const float foam = smoothstep(
        0.76f, 0.94f, crestAmount * 0.72f + foamNoiseA * 0.2f + foamNoiseB * 0.08f);
	const float shoreFoamBand = smoothstep(0.03f, 0.12f, waterDepth)
		* (1.0f - smoothstep(0.12f, shorelineWidth, waterDepth));
	const float shoreFoam = shoreFoamBand * saturate(foamNoiseA * 0.7f + foamNoiseB * 0.3f);
    waterColor = lerp(
        waterColor, max(directionalRadiance, 0.65f),
		saturate(foam * saturate(waveStrength) + shoreFoam * 0.65f));

    const float edgeDistance = min(
        min(input.uv.x, 1.0f - input.uv.x),
        min(input.uv.y, 1.0f - input.uv.y));
    const float outerEdgeFade = smoothstep(0.0f, 0.08f, edgeDistance);
    const float alpha = saturate(
		opacity * (0.72f + fresnel * 0.28f) + foam * 0.18f + shoreFoam * 0.12f)
		* shorelineFade * outerEdgeFade;
    return float4(saturate(waterColor), alpha);
}
