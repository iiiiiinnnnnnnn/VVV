// TerrainPrimitiveMeshVS.hlsl

#include "TerrainPrimitive.hlsli"
#include "ShadowmapFunctions.hlsli"

VS_OUT main(float3 position : POSITION, float3 normal : NORMAL, float2 texcoord : TEXCOORD)
{
    VS_OUT output = (VS_OUT)0;

    float4 worldPosition = mul(float4(position, 1.0f), world);
    float3 worldNormal = normalize(mul(normal, (float3x3)world));
    float3 worldTangent = normalize(mul(float3(1.0f, 0.0f, 0.0f), (float3x3)world));

    output.vertex = mul(worldPosition, viewProjection);
    output.texcoord = texcoord;
    output.normal = worldNormal;
    output.position = worldPosition.xyz;
    output.tangent = worldTangent;
    output.shadowTexcoord = CalcShadowTexcoord(
        worldPosition.xyz,
        light_view_projection);

    return output;
}
