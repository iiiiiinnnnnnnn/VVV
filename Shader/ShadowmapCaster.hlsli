// ShadowmapCaster.hlsli

#include "Skinning.hlsli"

// シャドウマップパス専用のシーン定数バッファ
// Scene.hlsliのb7(CbScene)は通常描画パス用なので使わない。
// ShadowMapRenderer が VS slot7 に lightViewProjection だけを詰める。
cbuffer CbShadowScene : register(b7)
{
    row_major float4x4 lightViewProjection;
};
