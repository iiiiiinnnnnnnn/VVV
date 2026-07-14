// ShadowMapRenderer.cpp

#include "Rendering/Renderer/ShadowMapRenderer.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Resource/GpuResourceUtils.h"
#include "Gameplay/Stage/Component/Terrain.h"

ShadowMapRenderer::ShadowMapRenderer(ID3D11Device* device, UINT shadowMapSize)
    : shadowMapSize(shadowMapSize)
{
    HRESULT hr;

    // ----- デプスオンリーテクスチャの生成 -----
    // シャドウマップはデプスのみ。SRVとして後でPBRShaderに渡す。
    // フォーマット:
    //   テクスチャ本体  → DXGI_FORMAT_R32_TYPELESS  (DSVとSRV両方に使える)
    //   DSV            → DXGI_FORMAT_D32_FLOAT
    //   SRV            → DXGI_FORMAT_R32_FLOAT
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = shadowMapSize;
        desc.Height = shadowMapSize;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        hr = device->CreateTexture2D(&desc, nullptr, tex.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        // DSV
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = device->CreateDepthStencilView(tex.Get(), &dsvDesc, depthDsv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        // SRV（PBRShaderのslot8にバインドする）
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        hr = device->CreateShaderResourceView(tex.Get(), &srvDesc, depthSrv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    // ビューポート
    shadowViewport.Width = static_cast<float>(shadowMapSize);
    shadowViewport.Height = static_cast<float>(shadowMapSize);
    shadowViewport.MinDepth = 0.0f;
    shadowViewport.MaxDepth = 1.0f;
    shadowViewport.TopLeftX = 0.0f;
    shadowViewport.TopLeftY = 0.0f;

    GpuResourceUtils::LoadVertexShader(
        device,
        "Data/Shader/ShadowMapCasterVS.cso",
        ModelShader::InputElementDescs.data(),
        static_cast<UINT>(ModelShader::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    // 6
    GpuResourceUtils::CreateConstantBuffer(
        device,
        sizeof(CbSkeleton),
        skeletonConstantBuffer.GetAddressOf());

    // 7
    GpuResourceUtils::CreateConstantBuffer(
        device,
        sizeof(CbShadowScene),
        shadowSceneConstantBuffer.GetAddressOf());

    // ----- ラスタライザーステート -----
    {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_FRONT;
        desc.FrontCounterClockwise = false;
        desc.DepthBias = 1000;
        desc.SlopeScaledDepthBias = 2.0f;
        desc.DepthBiasClamp = 0.0f;
        desc.DepthClipEnable = true;
        hr = device->CreateRasterizerState(&desc, rasterizerState.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        desc.CullMode = D3D11_CULL_NONE;
        hr = device->CreateRasterizerState(&desc, terrainRasterizerState.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    // ----- デプスステンシルステート -----
    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = true;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        hr = device->CreateDepthStencilState(&desc, depthStencilState.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }
}

void ShadowMapRenderer::Draw(Model* model)
{
    drawList.emplace_back(model);
}

void ShadowMapRenderer::Draw(Terrain* terrain)
{
    terrainDrawList.emplace_back(terrain);
}

void ShadowMapRenderer::Render(const RenderContext& rc,
                               const Vector3& lightDir,
                               const Vector3& targetPos,
                               float lightDistance,
                               float orthoSize,
                               float nearZ,
                               float farZ)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    // ----- ライト行列計算 -----
    {
        // ライトを方向の逆から照らす位置に置く
        Vector3 lightPos = targetPos - lightDir * lightDistance;

        // 正射影（平行光源なので透視投影ではなく正射影）
        // orthoSizeでシーン全体がカバーされるよう調整する
        Matrix V = Matrix::CreateLookAt(lightPos, targetPos, Vector3::Up);
        Matrix P = Matrix::CreateOrthographic(orthoSize, orthoSize, nearZ, farZ);
        lightViewProjection = V * P;

        // ライト行列定数バッファ更新
        CbShadowScene cbShadowScene{};
        cbShadowScene.lightViewProjection = lightViewProjection;
        dc->UpdateSubresource(shadowSceneConstantBuffer.Get(), 0, 0, &cbShadowScene, 0, 0);
    }

    // ----- RenderTarget切り替え -----
    // カラーRTはnull（デプスのみ書き込み）
    // 前のRTは保存せず上書き。Deactivateは呼び出し元がGraphics::SetRenderTargets()で戻す前提
    {
        ID3D11RenderTargetView* nullRtv = nullptr;
        dc->OMSetRenderTargets(1, &nullRtv, depthDsv.Get());
        dc->RSSetViewports(1, &shadowViewport);
        dc->ClearDepthStencilView(depthDsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    }

    // ----- シェーダー＆ステート設定 -----
    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0); // デプスのみ
    dc->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // ブレンド無効
    dc->OMSetDepthStencilState(depthStencilState.Get(), 0);
    dc->RSSetState(rasterizerState.Get());

    // 定数バッファ設定
    // VS slot6: スケルトン（ModelRendererと同じスロット番号で統一）
    // VS slot7: ライトViewProjection
    ID3D11Buffer* vsCbs[] =
    {
        skeletonConstantBuffer.Get(),       // slot 6
        shadowSceneConstantBuffer.Get(),    // slot 7
    };
    dc->VSSetConstantBuffers(6, _countof(vsCbs), vsCbs);

    // ----- メッシュ描画 -----
    for (const auto& model : drawList)
    {
        for (const Model::Mesh& mesh : model->GetMeshes())
        {
            if (!mesh.isDraw) continue;

            // スケルトン定数バッファ更新
            CbSkeleton cbSkeleton{};
            if (mesh.bones.size() > 0)
            {
                for (size_t i = 0; i < mesh.bones.size(); ++i)
                {
                    const Model::Bone& bone = mesh.bones.at(i);
                    cbSkeleton.boneTransforms[i] =
                        bone.offsetTransform * bone.node->worldTransform;
                }
            }
            else
            {
                cbSkeleton.boneTransforms[0] = mesh.node->worldTransform;
            }
            dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);

            // 頂点バッファ・インデックスバッファ設定
            UINT stride = sizeof(Model::Vertex);
            UINT offset = 0;
            dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
            dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
        }
    }
    drawList.clear();

    dc->RSSetState(terrainRasterizerState.Get());

    for (Terrain* terrain : terrainDrawList)
    {
        if (terrain != nullptr)
        {
            terrain->RenderShadowMap(dc, lightViewProjection);
        }
    }
    terrainDrawList.clear();

    // ----- 後始末 -----
    // SRVとしてPBRShaderで参照するため、DSVからは外しておく（同一リソースの同時バインド禁止）
    {
        ID3D11RenderTargetView* nullRtv = nullptr;
        ID3D11DepthStencilView* nullDsv = nullptr;
        dc->OMSetRenderTargets(1, &nullRtv, nullDsv);
    }

    ID3D11Buffer* nullCbs[] = {nullptr, nullptr};
    dc->VSSetConstantBuffers(6, _countof(nullCbs), nullCbs);
}
