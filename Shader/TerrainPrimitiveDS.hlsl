// TerrainPrimitiveDS.hlsl

#include "TerrainPrimitive.hlsli"
#include "ShadowmapFunctions.hlsli"

float SampleTerrainHeight(float2 uv)
{
    uv = saturate(uv);
    return terrainDataMap.SampleLevel(terrainPointClampSampler, uv, 0).r * height_scaler;
}

[domain("tri")]
VS_OUT main(
    HS_CONSTANT_OUT input,
    float3 barycentric : SV_DomainLocation,
    const OutputPatch<DS_IN, 3> patch)
{
    VS_OUT output = (VS_OUT)0;

    float3 localPosition =
        patch[0].position * barycentric.x +
        patch[1].position * barycentric.y +
        patch[2].position * barycentric.z;

    float2 texcoord =
        patch[0].texcoord * barycentric.x +
        patch[1].texcoord * barycentric.y +
        patch[2].texcoord * barycentric.z;

    localPosition.y += SampleTerrainHeight(texcoord);

    float texel = height_map_texel_size;
    float hL = SampleTerrainHeight(texcoord - float2(texel, 0.0f));
    float hR = SampleTerrainHeight(texcoord + float2(texel, 0.0f));
    float hD = SampleTerrainHeight(texcoord - float2(0.0f, texel));
    float hU = SampleTerrainHeight(texcoord + float2(0.0f, texel));

    float texelWorldSize = terrain_size * texel;

    float3 localNormal = normalize(float3(
        hL - hR,
        2.0f * texelWorldSize,
        hD - hU));

    float3 localTangent = normalize(float3(
        2.0f * texelWorldSize,
        hR - hL,
        0.0f));

    float4 worldPosition = mul(float4(localPosition, 1.0f), world);
    float3 worldNormal = normalize(mul(localNormal, (float3x3)world));
    float3 worldTangent = normalize(mul(localTangent, (float3x3)world));

    output.vertex = mul(worldPosition, viewProjection);
    output.texcoord = texcoord;
    output.normal = worldNormal;
    output.position = worldPosition.xyz;
    output.tangent = worldTangent;
    return output;
}
