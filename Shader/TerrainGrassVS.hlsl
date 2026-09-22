#include "TerrainGrass.hlsli"

GrassGSInput main(GrassVSInput input)
{
    GrassGSInput output;
    output.position = input.position;
    output.normal = input.normal;
    output.random = input.random;
    return output;
}
