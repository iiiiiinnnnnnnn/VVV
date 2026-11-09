#include "Resource/ResourceManager.h"
#include <Dx11Kit/Dx11Kit.h>
#include "Rendering/Core/Graphics.h"
#include <ProfileKit/ProfileKit.h>
#include "Rendering/Renderer/SkyBoxRenderer.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Rendering/Core/RenderContext.h"
#include <imgui.h>

SkyBoxRenderer::SkyBoxRenderer(ID3D11Device* device)
{
    // 頂点シェーダー（入力レイアウト不要 = SV_VertexID 使用）
    vertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/SkyBoxVS.cso").c_str());

    // ピクセルシェーダー
    pixelShader = std::make_unique<Gpu::PixelShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/SkyBoxPS.cso").c_str());

    // 定数バッファ
    constantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbSkyBox));

    // デプスステート: TestOnly z=1 が通るように LESS_EQUAL
    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable    = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 書かない
        desc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL; // z=1.0 が通る
        depthStencilState = std::make_unique<Gpu::DepthStencilState>(device, desc);
    }

    // ラスタライザー: CullNone
    {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.DepthClipEnable = TRUE;
        rasterizerState = std::make_unique<Gpu::RasterizerState>(device, desc);
    }
}

void SkyBoxRenderer::Render(ID3D11DeviceContext* dc,
                             const Game::Graphics* graphicsResources,
                             const Camera& camera,
                             ID3D11ShaderResourceView* skyTex,
							 const RenderSettings& renderSettings)
{
    PROFILE_FUNCTION();
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
        constantBuffer->Update(dc, &cb);
    }

    // ステート設定
    Gpu::VertexShader::UnbindInputLayout(dc);
    Gpu::VertexBuffer::SetTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Gpu::IndexBuffer::Unbind(dc, DXGI_FORMAT_UNKNOWN);

    vertexShader->Bind(dc);
    pixelShader->Bind(dc);

    constantBuffer->BindPS(dc, 0);
    Gpu::PixelShader::SetShaderResources(dc, 0, skyTex);

    graphicsResources->LinearWrapSamplerState->BindPS(dc, 0);

    depthStencilState->Bind(dc);
    rasterizerState->Bind(dc);
    graphicsResources->OpaqueBlendState->Bind(dc);

    // 頂点バッファなしで3頂点描画
    vertexShader->Draw(dc, 3);

    // 後始末
    Gpu::VertexShader::Unbind(dc);
    Gpu::PixelShader::Unbind(dc);
    Gpu::VertexShader::UnbindInputLayout(dc);
    Gpu::PixelShader::UnbindShaderResources(dc, 0);
    Gpu::ConstantBuffer::UnbindPS(dc, 0);
}

void SkyBoxRenderer::DrawGUI()
{
    ImGui::DragFloat("Sky Intensity", &skyboxData.skyIntensity, 0.1f, 0.0f, 10.0f);
}
