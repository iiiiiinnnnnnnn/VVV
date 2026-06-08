// TrailRendererPS.hlsl

#include "TrailRenderer.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    float alpha = pow(saturate(1.0 - pin.uv.y), 2.0);
    alpha *= lerp(1.0, 0.3, pin.uv.x);
    alpha *= 3.0; // ‚±‚­‚·‚é
    alpha = saturate(alpha);
    return float4(1.0, 0.9, 0.3, alpha);
}