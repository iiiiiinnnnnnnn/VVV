// GeometryParticleGS.hlsl

#include "GeometryParticle.hlsli"

//  1頂点から4頂点生成する
[maxvertexcount(4)]
void main(point GS_IN gin[1], inout TriangleStream<PS_IN> output)
{
    //  カメラの右/上方向へ広げてビルボードを作る
    float3 pos = gin[0].position;

	//  点を面にするため4頂点の座標を生成
    float rot = gin[0].param.x;
    float s = sin(rot);
    float c = cos(rot);
    float3 right = (cameraRight * c - cameraUp * s) * (gin[0].size.x * 0.5);
    float3 up = (cameraRight * s + cameraUp * c) * (gin[0].size.y * 0.5);

    //  4角形ポリゴンを生成
    float3 posLeftTop = pos - right + up;
    float3 posLeftBottom = pos - right - up;
    float3 posRightTop = pos + right + up;
    float3 posRightBottom = pos + right - up;

	//  UV座標の切り取り位置をtypeから算出
    uint type = (uint) gin[0].param.y;
    uint komax = (uint) gin[0].param.z;
    uint komay = (uint) gin[0].param.w;
    float w = 1.0 / komax;
    float h = 1.0 / komay;
    float2 uv = float2((type % komax) * w, (type / komax) * h);

	//  左上の点の位置(射影座標系)・UV・色を計算して出力
    PS_IN pout = (PS_IN) 0;
    pout.color = gin[0].color;
    pout.position = mul(float4(posLeftTop, 1.0f), viewProjection);
    pout.texcoord = uv + float2(0, 0); //   テクスチャ左上
    output.Append(pout);
    
	//  右上の点の位置(射影座標系) とテクスチャ座標の計算をして出力
    pout.color = gin[0].color;
    pout.position = mul(float4(posRightTop, 1.0f), viewProjection);
    pout.texcoord = uv + float2(w, 0); //  テクスチャ
    output.Append(pout);

	//  左下の点の位置(射影座標系) とテクスチャ座標の計算をして出力
    pout.color = gin[0].color;
    pout.position = mul(float4(posLeftBottom, 1.0f), viewProjection);
    pout.texcoord = uv + float2(0, h); //   テクスチャ
    output.Append(pout);

	//  右下の点の位置(射影座標系) とテクスチャ座標の計算をして出力
    pout.color = gin[0].color;
    pout.position = mul(float4(posRightBottom, 1.0f), viewProjection);
    pout.texcoord = uv + float2(w, h); //  テクスチャ
    output.Append(pout);

    output.RestartStrip();
}
