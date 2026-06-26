// TerrainColliderBuildCS.hlsl

#include "TerrainPrimitive.hlsli"

cbuffer CbTerrainColliderBuild : register(b0)
{
    float colliderTerrainSize;
    float colliderHeightMapTexelSize;
    float colliderHeightScaler;
    float colliderDummy0;
    int minGridX;
    int minGridZ;
    int segmentCountX;
    int segmentCountZ;
    int totalSegmentCountX;
    int totalSegmentCountZ;
    int vertexLineCount;
    int colliderDummy1;
};

RWStructuredBuffer<float4> colliderVertices : register(u0);

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint vertexCount = (uint)((segmentCountX + 1) * (segmentCountZ + 1));
    uint index = dispatchThreadId.x;
    if (index >= vertexCount)
    {
        return;
    }

    uint localX = index % (uint)vertexLineCount;
    uint localZ = index / (uint)vertexLineCount;

    uint gridX = (uint)minGridX + localX;
    uint gridZ = (uint)minGridZ + localZ;

    float u = (float)gridX / (float)totalSegmentCountX;
    float v = (float)gridZ / (float)totalSegmentCountZ;
    float height = terrainDataMap.SampleLevel(terrainPointClampSampler, saturate(float2(u, v)), 0).r * colliderHeightScaler;

    colliderVertices[index] = float4(
        (u - 0.5f) * colliderTerrainSize,
        height,
        (v - 0.5f) * colliderTerrainSize,
        1.0f);
}
