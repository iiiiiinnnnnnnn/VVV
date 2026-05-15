// ShadowmapCasterShader.cpp
// VVV 用シャドウマップキャスターシェーダークラス実装
// Ex04_Base から移植 (2026-05-15)

#include "Misc.h"
#include "GpuResourceUtils.h"
#include "ShadowmapCasterShader.h"

ShadowmapCasterShader::ShadowmapCasterShader(ID3D11Device* device)
{
    // 頂点シェーダー + 入力レイアウト
    GpuResourceUtils::LoadVertexShader(
        device,
        "Data/Shader/ShadowmapCasterVS.cso",
        Model::InputElementDescs.data(),
        static_cast<UINT>(Model::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    // キャスター用定数バッファ (b9)
    GpuResourceUtils::CreateConstantBuffer(
        device,
        sizeof(CbShadowmapCaster),
        casterConstantBuffer.GetAddressOf());
}

void ShadowmapCasterShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);   // PS 不要

    // b6=CbSkeleton は ModelRenderer が設定済み
    // b9=CbShadowmapCaster を更新してバインド
    CbShadowmapCaster cb = {};
    cb.lightViewProjection = lightViewProjection;
    dc->UpdateSubresource(casterConstantBuffer.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11Buffer* vsCbs[] = { casterConstantBuffer.Get() };
    dc->VSSetConstantBuffers(9, _countof(vsCbs), vsCbs);
}

void ShadowmapCasterShader::Update(const RenderContext& rc, const Model::Mesh& /*mesh*/)
{
    // キャスターはマテリアル情報不要
}

void ShadowmapCasterShader::End(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    dc->VSSetShader(nullptr, nullptr, 0);
    dc->IASetInputLayout(nullptr);

    ID3D11Buffer* nullCbs[] = { nullptr };
    dc->VSSetConstantBuffers(9, _countof(nullCbs), nullCbs);
}
