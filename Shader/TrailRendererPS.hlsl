// TrailRendererPS.hlsl

#include "TrailRenderer.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    // UV.y : 0=V‚µ‚¢’[, 1=ŒÃ‚¢’[ ¨ ŒÃ‚¢‚Ù‚Ç“§–¾
    float alpha = pow(saturate(1.0 - pin.uv.y), 2.0);

    // UV.x : 0=root, 1=tip ¨ tip‘¤‚ğ­‚µ×‚­
    alpha *= lerp(1.0, 0.3, pin.uv.x);

    return float4(1.0, 0.9, 0.3, alpha);
}