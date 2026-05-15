// PBRVS.hlsl
// VVV 用 PBR 頂点シェーダー
// Ex04_Base から移植 (2026-05-15)

#include "PBR.hlsli"

VS_OUT main(
    float4 position     : POSITION,
    float3 normal       : NORMAL,
    float4 tangent      : TANGENT,   // VVV の Model::Vertex は float4 tangent
    float2 texcoord     : TEXCOORD,
    float4 boneWeights  : BONE_WEIGHTS,
    uint4  boneIndices  : BONE_INDICES
)
{
    // スキニング
    float4 skinnedPos    = SkinningPosition(position,    boneWeights, boneIndices);
    float3 skinnedNormal = SkinningVector(normal,        boneWeights, boneIndices);
    float3 skinnedTangent= SkinningVector(tangent.xyz,   boneWeights, boneIndices);

    VS_OUT vout;
    vout.vertex    = mul(skinnedPos, viewProjection);
    vout.position  = skinnedPos.xyz;
    vout.normal    = normalize(skinnedNormal);
    vout.tangent   = normalize(skinnedTangent);
    vout.texcoord  = texcoord;

    // シャドウマップ参照用情報を算出
    vout.shadowTexcoord = CalcShadowTexcoord(skinnedPos.xyz, lightViewProjection);

    return vout;
}
