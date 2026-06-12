// TerrainPrimitiveDS.hlsl

#include "TerrainPrimitive.hlsli"

float SampleTerrainHeight(float2 uv)
{
    uv = saturate(uv);
    return terrainDataMap.SampleLevel(pointClampSampler, uv, 0).r * height_scaler;
}

[domain("tri")]
DS_OUT main(
    HS_CONSTANT_OUT input,
    float3 barycentric : SV_DomainLocation,
    const OutputPatch<DS_IN, 3> patch)
{
    DS_OUT output = (DS_OUT) 0;

    float3 localPosition =
        patch[0].position * barycentric.x +
        patch[1].position * barycentric.y +
        patch[2].position * barycentric.z;

    float2 texcoord =
        patch[0].texcoord * barycentric.x +
        patch[1].texcoord * barycentric.y +
        patch[2].texcoord * barycentric.z;

    float height = SampleTerrainHeight(texcoord);
    localPosition.y += height;

    float texel = height_map_texel_size;
    float hL = SampleTerrainHeight(texcoord - float2(texel, 0.0f));
    float hR = SampleTerrainHeight(texcoord + float2(texel, 0.0f));
    float hD = SampleTerrainHeight(texcoord - float2(0.0f, texel));
    float hU = SampleTerrainHeight(texcoord + float2(0.0f, texel));

    float texelWorldSize = terrain_size * texel;
    float3 localNormal = normalize(float3(
        hL - hR,
        2.0f * texelWorldSize,
        hD - hU
    ));

    float4 worldPosition = mul(float4(localPosition, 1.0f), world);
    float3 worldNormal = normalize(mul(localNormal, (float3x3) world));

    output.position = mul(worldPosition, viewProjection);
    output.normal = worldNormal;
    output.texcoord = texcoord;
    output.worldPosition = worldPosition.xyz;

    return output;
}