// SkyBoxRenderer.cpp

#include "Rendering/Renderer/SkyBoxRenderer.h"
#include "Resource/GpuResourceUtils.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Rendering/Core/RenderContext.h"
#include <imgui.h>

SkyBoxRenderer::SkyBoxRenderer(ID3D11Device* device)
{
    HRESULT hr;

    // 頂点シェーダー（入力レイアウト不要 = SV_VertexID 使用）
    hr = GpuResourceUtils::LoadVertexShader(
        device,
        "Data/Shader/SkyBoxVS.cso",
        nullptr, 0,
        nullptr,
        vertexShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // ピクセルシェーダー
    hr = GpuResourceUtils::LoadPixelShader(
        device,
        "Data/Shader/SkyBoxPS.cso",
        pixelShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // 定数バッファ
    hr = GpuResourceUtils::CreateConstantBuffer(
        device,
        sizeof(CbSkyBox),
        constantBuffer.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // デプスステート: TestOnly z=1 が通るように LESS_EQUAL
    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable    = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 書かない
        desc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL; // z=1.0 が通る
        hr = device->CreateDepthStencilState(&desc, depthStencilState.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    // ラスタライザー: CullNone
    {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.DepthClipEnable = TRUE;
        hr = device->CreateRasterizerState(&desc, rasterizerState.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }
}

void SkyBoxRenderer::Render(ID3D11DeviceContext* dc,
                             const RenderState* renderState,
                             const Camera& camera,
                             ID3D11ShaderResourceView* skyTex,
							 const RenderSettings& renderSettings)
{
    if (!skyTex) return;

    // 定数バッファ更新
    {
        Matrix VP = camera.GetView() * camera.GetProjection();
        Matrix invVP;
        VP.Invert(invVP);

        CbSkyBox cb{};
        cb.inverseViewProjection = invVP;
        cb.viewPos               = camera.GetEye();
        cb.skyIntensity          = skyboxData.skyIntensity;
		cb.distanceFogColor = renderSettings.distanceFogColor;
		cb.distanceFogParams = {
			renderSettings.distanceFogStrength,
			renderSettings.distanceFogEnabled ? 1.0f : 0.0f,
			0.0f,
			0.0f};
        dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &cb, 0, 0);
    }

    // ステート設定
    dc->IASetInputLayout(nullptr);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    dc->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);

    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);

    dc->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    dc->PSSetShaderResources(0, 1, &skyTex);

    ID3D11SamplerState* samp = renderState->GetSamplerState(SamplerState::LinearWrap);
    dc->PSSetSamplers(0, 1, &samp);

    dc->OMSetDepthStencilState(depthStencilState.Get(), 0);
    dc->RSSetState(rasterizerState.Get());
    dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

    // 頂点バッファなしで3頂点描画
    dc->Draw(3, 0);

    // 後始末
    dc->VSSetShader(nullptr, nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);
    dc->IASetInputLayout(nullptr);
    ID3D11ShaderResourceView* nullSrv = nullptr;
    dc->PSSetShaderResources(0, 1, &nullSrv);
    ID3D11Buffer* nullCb = nullptr;
    dc->PSSetConstantBuffers(0, 1, &nullCb);
}

void SkyBoxRenderer::DrawGUI()
{
    ImGui::DragFloat("Sky Intensity", &skyboxData.skyIntensity, 0.1f, 0.0f, 10.0f);
}
