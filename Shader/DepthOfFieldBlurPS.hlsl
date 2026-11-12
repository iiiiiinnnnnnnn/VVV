#include "DepthOfField.hlsli"

Texture2D sceneColor : register(t0);
SamplerState linearClampSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    // 中心と左右 4 点の対称なガウス重み。合計を 1 に揃え、ぼかしによる明るさの変化を抑える
    static const float weights[5] = {0.2270270270f, 0.1945945946f, 0.1216216216f, 0.0540540541f, 0.0162162162f};
    float4 color = sceneColor.Sample(linearClampSampler, pin.texcoord) * weights[0];

    // blurStep の向きを変えて同じシェーダーを横・縦の 2 パスに使う
    [unroll]
    for (int i = 1; i < 5; ++i)
    {
        float2 offset = blurStep * i;
        color += sceneColor.Sample(linearClampSampler, pin.texcoord + offset) * weights[i];
        color += sceneColor.Sample(linearClampSampler, pin.texcoord - offset) * weights[i];
    }
    return color;
}
