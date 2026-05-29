// ShadowmapCasterVS.hlsl

#include "ShadowmapCaster.hlsli"

float4 main(
    float4 position     : POSITION,
    float3 normal       : NORMAL,
    float4 tangent      : TANGENT,
    float2 texcoord     : TEXCOORD,
    float4 boneWeights  : BONE_WEIGHTS,  // Shader.cppのInputElementDescsに合わせる
    uint4  boneIndices  : BONE_INDICES   // 同上
) : SV_POSITION
{
    // スキニングしてワールド空間へ（boneTransformsにワールド変換が含まれる）
    float4 worldPos = SkinningPosition(position, boneWeights, boneIndices);

    // ライトのViewProjectionでクリップ空間へ
    return mul(float4(worldPos.xyz, 1.0f), lightViewProjection);
}
