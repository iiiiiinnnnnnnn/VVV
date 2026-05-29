// PBRVS.hlsl

#include "Skinning.hlsli"
#include "PBR.hlsli"

VS_OUT main(
    float4 position     : POSITION,
    float3 normal       : NORMAL,
    float4 tangent      : TANGENT,
    float2 texcoord     : TEXCOORD,
    float4 boneWeights  : BONE_WEIGHTS,
    uint4  boneIndices  : BONE_INDICES)
{
    VS_OUT vout = (VS_OUT)0;

    // スキニング（boneTransformsにワールド変換が含まれているのでworld行列は不要）
    float4 worldPos      = SkinningPosition(position, boneWeights, boneIndices);
    float3 worldNormal   = SkinningVector(normal, boneWeights, boneIndices);
    float3 worldTangent  = SkinningVector(tangent.xyz, boneWeights, boneIndices);

    // クリップ空間
    vout.vertex   = mul(float4(worldPos.xyz, 1.0f), viewProjection);
    vout.position = worldPos.xyz;   // ワールド座標をPSに渡す

    vout.normal   = normalize(worldNormal);
    vout.tangent  = normalize(worldTangent);
    vout.texcoord = texcoord;

    // シャドウマップ用テクスチャ座標をVSで計算してPSに渡す
    {
        float4 lightClip = mul(float4(worldPos.xyz, 1.0f), light_view_projection);
        lightClip /= lightClip.w;
        lightClip.y = -lightClip.y;
        lightClip.xy = 0.5f * lightClip.xy + 0.5f;
        vout.shadowTexcoord = lightClip.xyz;
    }

    return vout;
}
