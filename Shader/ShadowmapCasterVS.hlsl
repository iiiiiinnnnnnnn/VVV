// ShadowmapCasterVS.hlsl
// VVV 用シャドウマップキャスター頂点シェーダー
// Ex04_Base から移植 (2026-05-15)

#include "ShadowmapCaster.hlsli"
#include "Skinning.hlsli"

float4 main(
    float4 position     : POSITION,
    float3 normal       : NORMAL,
    float4 tangent      : TANGENT,
    float2 texcoord     : TEXCOORD,
    float4 boneWeights  : BONE_WEIGHTS,
    uint4  boneIndices  : BONE_INDICES
) : SV_POSITION
{
    float4 skinnedPos = SkinningPosition(position, boneWeights, boneIndices);
    return mul(skinnedPos, lightViewProjection);
}
