#include "DepthOfField.hlsli"

Texture2D sceneColor : register(t0);
Texture2D blurredColor : register(t1);
Texture2D sceneDepth : register(t2);
SamplerState linearClampSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    // 輪郭をまたいだ補間深度から架空の距離を作らないよう、深度は画素単位で読む
    uint width, height;
    sceneDepth.GetDimensions(width, height);
    int2 pixel = clamp(int2(pin.texcoord * float2(width, height)), int2(0, 0), int2(width, height) - 1);
    float depth = sceneDepth.Load(int3(pixel, 0)).r;

    // UV の Y は下向き、NDC は上向き。逆射影後に w で割ってビュー空間の Z を取り出す
    // カメラからの直線距離ではなく前方への距離なので、ピント面は画面に平行になる
    float2 ndc = pin.texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 view = mul(float4(ndc, depth, 1.0f), inverseProjection);
    float viewDistance = abs(view.z) / max(abs(view.w), 0.000001f);

    // ピントより手前・奥で強度を選び、ピントから離れるほどぼかし画像の割合を増やす
    float offset = viewDistance - focusDistance;
    float strength = offset < 0.0f ? nearStrength : farStrength;
    float blend = saturate(abs(offset) / max(focusRange, 0.001f) * strength);

    // ぼかし自体は深度を区別しないため、物体の境界では周囲の色がにじむ簡易方式
    float4 original = sceneColor.Sample(linearClampSampler, pin.texcoord);
    float3 blurred = blurredColor.Sample(linearClampSampler, pin.texcoord).rgb;
    return float4(lerp(original.rgb, blurred, blend), original.a);
}
