// ColorFilterPS.hlsl

#include "FullScreenQuad.hlsli"

cbuffer CbColorFilter : register(b2)
{
    float hueShift;
    float saturation;
    float brightness;
    float DUMMY;
};

Texture2D sceneMap : register(t0);
SamplerState linearSamplerState : register(s0);

float3 RGBToHSV(float3 rgb)
{
    const float Maximum = max(rgb.r, max(rgb.g, rgb.b));
    const float Minimum = min(rgb.r, min(rgb.g, rgb.b));
    const float Delta = Maximum - Minimum;

    float hue = 0.0f;
    if (Delta > 0.000001f)
    {
        if (Maximum == rgb.r)
            hue = 60.0f * fmod((rgb.g - rgb.b) / Delta, 6.0f);
        else if (Maximum == rgb.g)
            hue = 60.0f * ((rgb.b - rgb.r) / Delta + 2.0f);
        else
            hue = 60.0f * ((rgb.r - rgb.g) / Delta + 4.0f);

        if (hue < 0.0f) hue += 360.0f;
    }

    const float ValueSaturation = Maximum > 0.000001f ? Delta / Maximum : 0.0f;
    return float3(hue, ValueSaturation, Maximum);
}

float3 HSVToRGB(float3 hsv)
{
    const float Hue = fmod(hsv.x, 360.0f) / 60.0f;
    const float Chroma = hsv.z * hsv.y;
    const float X = Chroma * (1.0f - abs(fmod(Hue, 2.0f) - 1.0f));
    const float Match = hsv.z - Chroma;

    float3 rgb;
    if (Hue < 1.0f) rgb = float3(Chroma, X, 0.0f);
    else if (Hue < 2.0f) rgb = float3(X, Chroma, 0.0f);
    else if (Hue < 3.0f) rgb = float3(0.0f, Chroma, X);
    else if (Hue < 4.0f) rgb = float3(0.0f, X, Chroma);
    else if (Hue < 5.0f) rgb = float3(X, 0.0f, Chroma);
    else rgb = float3(Chroma, 0.0f, X);

    return rgb + Match;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = sceneMap.Sample(linearSamplerState, pin.texcoord);
    float3 hsv = RGBToHSV(color.rgb);
    hsv.x = fmod(hsv.x + hueShift, 360.0f);
    hsv.y = saturate(hsv.y * saturation);
    hsv.z = max(hsv.z * brightness, 0.0f);
    color.rgb = HSVToRGB(hsv);
    return color;
}
