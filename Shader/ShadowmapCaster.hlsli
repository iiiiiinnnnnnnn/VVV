// ShadowmapCaster.hlsli
// VVV 用シャドウマップキャスター定数バッファ定義
// Ex04_Base から移植 (2026-05-15)
//
// ライトビュープロジェクションは PBRShader の CbShadowmap (b9) と共有するのでなく、
// キャスター側は独立したバッファ (b9) として同じスロットに置く。

cbuffer CbShadowmapCaster : register(b9)
{
    row_major float4x4 lightViewProjection;
};
