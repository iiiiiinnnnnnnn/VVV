// TrailRendererPS.hlsl

#include "TrailRenderer.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    float alpha = pow(saturate(1.0 - pin.uv.y), 2.0);
    alpha *= lerp(1.0, 0.3, pin.uv.x);
    alpha *= 3.0; // こくする
    alpha = saturate(alpha);
    float4 gradient = lerp(color, endColor, saturate(pin.uv.y));
    return float4(gradient.rgb, alpha * gradient.a);
}
