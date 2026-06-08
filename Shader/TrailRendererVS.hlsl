// TrailRendererVS.hlsl

#include "TrailRenderer.hlsli"

VS_OUT main(VS_IN vin)
{
	VS_OUT vout;
	vout.position = mul(vin.position, viewProjection);
    vout.uv = vin.uv;

	return vout;
}
