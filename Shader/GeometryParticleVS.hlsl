// GeometryParticleVS.hlsl

#include "GeometryParticle.hlsli"

GS_IN main(VS_IN vin)
{
    GS_IN output = (GS_IN) 0;
    //  ’¸“_î•ñ‚ğ‚»‚Ì‚Ü‚Ü‘—‚é
    output.position = vin.position;
    output.color = vin.color;
    output.size = vin.size;
    output.param = vin.param;
    return output;
}
