// TerrainPrimitivePS.hlsl

#include "TerrainPrimitive.hlsli"

Texture2D<float4> rockTexture : register(t20);
Texture2D<float4> dirtTexture : register(t21);
Texture2D<float4> grassTexture : register(t22);

float4 main(DS_OUT input) : SV_TARGET
{
    float2 tilingCoord = input.texcoord * tilling_scale;

    float4 rockColor = rockTexture.Sample(linearWrapSampler, tilingCoord);
    float4 dirtColor = dirtTexture.Sample(linearWrapSampler, tilingCoord);
    float4 grassColor = grassTexture.Sample(linearWrapSampler, tilingCoord);

    float blendRate = saturate(terrainDataMap.Sample(linearClampSampler, input.texcoord).g);

    float4 color = lerp(rockColor, dirtColor, smoothstep(0.0f, 0.5f, blendRate));
    color = lerp(color, grassColor, smoothstep(0.5f, 1.0f, blendRate));

    float3 normal = normalize(input.normal);
    float3 lightDirection = normalize(-directionalLightDirection);

    float diffuse = saturate(dot(normal, lightDirection));
    float3 lighting = ambientColor.rgb * 0.35f + directionalLightColor.rgb * (0.35f + diffuse * 0.65f);

    return float4(color.rgb * lighting, color.a);
}