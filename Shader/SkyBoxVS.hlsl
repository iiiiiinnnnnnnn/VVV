// SkyBoxVS.hlsl
// フルスクリーントライアングル1枚で描画（頂点バッファ不要）

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 ndcXY    : TEXCOORD0;  // NDC XY をPSに渡す
};

VS_OUT main(uint vertexId : SV_VertexID)
{
    // 画面全体を覆うトライアングル
    float2 ndc[3] =
    {
        float2(-1.0f,  1.0f),
        float2( 3.0f,  1.0f),
        float2(-1.0f, -3.0f),
    };

    VS_OUT vout;
    vout.position = float4(ndc[vertexId], 1.0f, 1.0f); // z=1: 最遠デプス
    vout.ndcXY    = ndc[vertexId];
    return vout;
}
