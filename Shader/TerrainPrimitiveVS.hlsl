// TerrainPrimitiveVS.hlsl

#include "TerrainPrimitive.hlsli"

HS_IN main(float3 position : POSITION, float3 normal : NORMAL, float2 texcoord : TEXCOORD)
{
    HS_IN output = (HS_IN) 0;
    output.position = position;
    output.normal = normal;
    output.texcoord = texcoord;
    return output;
}