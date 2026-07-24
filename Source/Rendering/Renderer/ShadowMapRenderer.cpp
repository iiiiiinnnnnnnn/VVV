// ShadowMapRenderer.cpp

#include "Rendering/Renderer/ShadowMapRenderer.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Resource/GpuResourceUtils.h"
#include "Gameplay/Stage/Component/Terrain.h"

#include <cfloat>
#include <cmath>

ShadowMapRenderer::ShadowMapRenderer(ID3D11Device* device, UINT shadowMapSize)
    : shadowMapSize(shadowMapSize)
{
    HRESULT hr;

    // ----- デプスオンリーテクスチャの生成 -----
    // シャドウマップはデプスのみ。SRVとして後でVMatShaderに渡す。
    // フォーマット:
    //   テクスチャ本体  → DXGI_FORMAT_R32_TYPELESS  (DSVとSRV両方に使える)
    //   DSV            → DXGI_FORMAT_D32_FLOAT
    //   SRV            → DXGI_FORMAT_R32_FLOAT
    for (int cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
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
        hr = device->CreateDepthStencilView(
            tex.Get(),
            &dsvDesc,
            depthDsvs[cascadeIndex].GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        // SRV（VMatShaderのslot8にバインドする）
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        hr = device->CreateShaderResourceView(
            tex.Get(),
            &srvDesc,
            depthSrvs[cascadeIndex].GetAddressOf());
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

void ShadowMapRenderer::Draw(VMDLModel* model)
{
    drawList.emplace_back(model);
}

void ShadowMapRenderer::Draw(Terrain* terrain)
{
    terrainDrawList.emplace_back(terrain);
}

void ShadowMapRenderer::Render(const RenderContext& rc,
                               const Vector3& lightDir,
                               float shadowDistance)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    if (!rc.camera)
    {
        drawList.clear();
        terrainDrawList.clear();
        return;
    }

    ID3D11ShaderResourceView* nullShadowSrvs[CascadeCount] = {};
    dc->PSSetShaderResources(8, CascadeCount, nullShadowSrvs);

    CalculateCascadeMatrices(*rc.camera, lightDir, shadowDistance);

    for (int cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        RenderCascade(dc, cascadeIndex);
    }

    drawList.clear();
    terrainDrawList.clear();

    ID3D11RenderTargetView* nullRtv = nullptr;
    dc->OMSetRenderTargets(1, &nullRtv, nullptr);

    ID3D11Buffer* nullCbs[] = {nullptr, nullptr};
    dc->VSSetConstantBuffers(6, _countof(nullCbs), nullCbs);
}

void ShadowMapRenderer::CalculateCascadeMatrices(
    const Camera& camera,
    const Vector3& lightDir,
    float shadowDistance)
{
    const Matrix& projection = camera.GetProjection();
    const float fovY = 2.0f * std::atan(1.0f / projection._22);
    const float aspect = projection._22 / projection._11;
    const float nearClip = -projection._43 / projection._33;
    const float cameraFarClip = -projection._43 / (projection._33 - 1.0f);
    const float farClip = std::min(shadowDistance, cameraFarClip);

    const float splitRatios[CascadeCount] = {0.05f, 0.15f, 0.40f, 1.0f};
    float splits[CascadeCount] = {};
    for (int i = 0; i < CascadeCount; ++i)
    {
        splits[i] = nearClip + (farClip - nearClip) * splitRatios[i];
    }
    cascadeSplits = Vector4(splits[0], splits[1], splits[2], splits[3]);

    Vector3 normalizedLightDir = lightDir;
    normalizedLightDir.Normalize();
    const Vector3 lightUp = std::abs(normalizedLightDir.Dot(Vector3::Up)) > 0.98f
        ? Vector3::Right
        : Vector3::Up;
    float cascadeNear = nearClip;

    for (int cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        const float cascadeFar = splits[cascadeIndex];
        const float nearHalfHeight = std::tan(fovY * 0.5f) * cascadeNear;
        const float nearHalfWidth = nearHalfHeight * aspect;
        const float farHalfHeight = std::tan(fovY * 0.5f) * cascadeFar;
        const float farHalfWidth = farHalfHeight * aspect;
        const Vector3 nearCenter = camera.GetEye() + camera.GetFront() * cascadeNear;
        const Vector3 farCenter = camera.GetEye() + camera.GetFront() * cascadeFar;

        const Vector3 corners[8] =
        {
            nearCenter + camera.GetUp() * nearHalfHeight + camera.GetRight() * nearHalfWidth,
            nearCenter + camera.GetUp() * nearHalfHeight - camera.GetRight() * nearHalfWidth,
            nearCenter - camera.GetUp() * nearHalfHeight + camera.GetRight() * nearHalfWidth,
            nearCenter - camera.GetUp() * nearHalfHeight - camera.GetRight() * nearHalfWidth,
            farCenter + camera.GetUp() * farHalfHeight + camera.GetRight() * farHalfWidth,
            farCenter + camera.GetUp() * farHalfHeight - camera.GetRight() * farHalfWidth,
            farCenter - camera.GetUp() * farHalfHeight + camera.GetRight() * farHalfWidth,
            farCenter - camera.GetUp() * farHalfHeight - camera.GetRight() * farHalfWidth,
        };

        Vector3 center = Vector3::Zero;
        for (const Vector3& corner : corners)
        {
            center += corner;
        }
        center /= 8.0f;

        float radius = 0.0f;
        for (const Vector3& corner : corners)
        {
            radius = std::max(radius, Vector3::Distance(center, corner));
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        const float depthPadding = radius;
        const Vector3 lightPosition = center - normalizedLightDir * (radius + depthPadding);
        const Matrix lightView = DirectX::XMMatrixLookAtLH(
            lightPosition,
            center,
            lightUp);

        Vector3 minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
        Vector3 maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (const Vector3& corner : corners)
        {
            const Vector3 lightSpace = Vector3::Transform(corner, lightView);
            minBounds.x = std::min(minBounds.x, lightSpace.x);
            minBounds.y = std::min(minBounds.y, lightSpace.y);
            minBounds.z = std::min(minBounds.z, lightSpace.z);
            maxBounds.x = std::max(maxBounds.x, lightSpace.x);
            maxBounds.y = std::max(maxBounds.y, lightSpace.y);
            maxBounds.z = std::max(maxBounds.z, lightSpace.z);
        }

        minBounds.z = std::max(0.01f, minBounds.z - depthPadding);
        maxBounds.z += depthPadding;

        const float worldUnitsPerTexel = (maxBounds.x - minBounds.x) / shadowMapSize;
        if (worldUnitsPerTexel > 0.0f)
        {
            minBounds.x = std::floor(minBounds.x / worldUnitsPerTexel) * worldUnitsPerTexel;
            minBounds.y = std::floor(minBounds.y / worldUnitsPerTexel) * worldUnitsPerTexel;
            maxBounds.x = std::ceil(maxBounds.x / worldUnitsPerTexel) * worldUnitsPerTexel;
            maxBounds.y = std::ceil(maxBounds.y / worldUnitsPerTexel) * worldUnitsPerTexel;
        }

        const Matrix lightProjection = DirectX::XMMatrixOrthographicOffCenterLH(
            minBounds.x,
            maxBounds.x,
            minBounds.y,
            maxBounds.y,
            minBounds.z,
            maxBounds.z);
        lightViewProjections[cascadeIndex] = lightView * lightProjection;
        cascadeNear = cascadeFar;
    }
}

void ShadowMapRenderer::RenderCascade(ID3D11DeviceContext* dc, int cascadeIndex)
{
    CbShadowScene cbShadowScene{};
    cbShadowScene.lightViewProjection = lightViewProjections[cascadeIndex];
    dc->UpdateSubresource(shadowSceneConstantBuffer.Get(), 0, nullptr, &cbShadowScene, 0, 0);

    // ----- RenderTarget切り替え -----
    // カラーRTはnull（デプスのみ書き込み）
    // 前のRTは保存せず上書き。Deactivateは呼び出し元がGraphics::SetRenderTargets()で戻す前提
    {
        ID3D11RenderTargetView* nullRtv = nullptr;
        dc->OMSetRenderTargets(1, &nullRtv, depthDsvs[cascadeIndex].Get());
        dc->RSSetViewports(1, &shadowViewport);
        dc->ClearDepthStencilView(
            depthDsvs[cascadeIndex].Get(),
            D3D11_CLEAR_DEPTH,
            1.0f,
            0);
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
        for (const VMDLModel::Mesh& mesh : model->GetMeshes())
        {
            if (!mesh.isDraw) continue;

            // スケルトン定数バッファ更新
            CbSkeleton cbSkeleton{};
            if (mesh.bones.size() > 0)
            {
                for (size_t i = 0; i < mesh.bones.size(); ++i)
                {
                    const VMDLModel::Bone& bone = mesh.bones.at(i);
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
            UINT stride = sizeof(VMDLModel::Vertex);
            UINT offset = 0;
            dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
            dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
        }
    }
    dc->RSSetState(terrainRasterizerState.Get());

    for (Terrain* terrain : terrainDrawList)
    {
        if (terrain) terrain->RenderShadowMap(dc, lightViewProjections[cascadeIndex]);
    }

    // ----- 後始末 -----
    // SRVとしてVMatShaderで参照するため、DSVからは外しておく（同一リソースの同時バインド禁止）
    {
        ID3D11RenderTargetView* nullRtv = nullptr;
        ID3D11DepthStencilView* nullDsv = nullptr;
        dc->OMSetRenderTargets(1, &nullRtv, nullDsv);
    }

}
