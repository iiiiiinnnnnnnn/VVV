#pragma once

// ShadowmapCasterShader.h
// VVV 用シャドウマップキャスターシェーダークラス
// Ex04_Base から移植 (2026-05-15)
//
// 使い方:
//   1) シャドウマップ用 RTVless/DSV にバインドして Begin() → Draw() → End()
//   2) 得られた ID3D11ShaderResourceView* を PBRShader::ShadowmapData::shadowMapSRV に渡す

#include "Shader.h"
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>

class ShadowmapCasterShader : public Shader
{
public:
    ShadowmapCasterShader(ID3D11Device* device);
    ~ShadowmapCasterShader() override = default;

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const Model::Mesh& mesh) override;
    void End(const RenderContext& rc) override;

    // ライトビュープロジェクション行列をセット
    void SetLightViewProjection(const DirectX::XMFLOAT4X4& lvp)
    {
        lightViewProjection = lvp;
    }

private:
    struct CbShadowmapCaster
    {
        DirectX::XMFLOAT4X4 lightViewProjection;
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        casterConstantBuffer; // b9

    DirectX::XMFLOAT4X4 lightViewProjection = {};
};
