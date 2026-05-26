// PBR_VS.hlsl

#include "Skinning.hlsli"
#include "PBR.hlsli"

cbuffer WORLD_CONSTANT_BUFFER : register(b0)
{
    row_major float4x4 world;
};

VS_OUT main(
	float4 position : POSITION,
	float4 boneWeights : BONE_WEIGHTS,
	uint4 boneIndices : BONE_INDICES,
	float2 texcoord : TEXCOORD,
	float3 normal : NORMAL,
	float4 tangent : TANGENT)
{
    VS_OUT vout = (VS_OUT) 0;

    // スキニング処理
    position = SkinningPosition(position, boneWeights, boneIndices);
    normal = SkinningVector(normal, boneWeights, boneIndices);
    tangent.xyz = SkinningVector(tangent.xyz, boneWeights, boneIndices);

    // ワールド座標計算
    float4 worldPos = float4(position.xyz, 1.0f);
    float4 worldNormal = float4(normal, 0.0f);
    float4 worldTangent = float4(tangent.xyz, 0.0f);

    worldPos = mul(worldPos, world);
    vout.position = mul(worldPos, viewProjection);
    vout.vertex = float4(vout.position, 1);

    vout.normal = normalize(mul(worldNormal, world).xyz);
    vout.tangent = normalize(mul(worldTangent, world).xyz);
    vout.texcoord = texcoord;

    // シャドウマップ用のパラメーター計算
    {
        // ライトから見たNDC座標を算出
        float4 lightPos = mul(worldPos, light_view_projection);
        // NDC座標からUV座標を算出する
        lightPos /= lightPos.w;
        lightPos.y = -lightPos.y;
        lightPos.xy = 0.5f * lightPos.xy + 0.5f;
        vout.shadow_texcoord = lightPos.xyz;
    }

    return vout;
}