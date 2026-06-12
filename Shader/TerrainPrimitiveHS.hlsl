// TerrainPrimitiveHS.hlsl

#include "TerrainPrimitive.hlsli"

HS_CONSTANT_OUT PatchConstant(
    InputPatch<HS_IN, 3> input,
    uint primitiveId : SV_PrimitiveID)
{
    HS_CONSTANT_OUT output = (HS_CONSTANT_OUT) 0;

    output.edge[0] = edge_factor;
    output.edge[1] = edge_factor;
    output.edge[2] = edge_factor;
    output.inside = inner_factor;

    return output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PatchConstant")]
HS_OUT main(
    InputPatch<HS_IN, 3> input,
    uint controlPointId : SV_OutputControlPointID,
    uint primitiveId : SV_PrimitiveID)
{
    HS_OUT output = (HS_OUT) 0;

    output.position = input[controlPointId].position;
    output.normal = input[controlPointId].normal;
    output.texcoord = input[controlPointId].texcoord;

    return output;
}