#include "Rendering/Core/PixelShader.h"
#include "Rendering/Core/RenderTarget.h"
#include "Rendering/Core/VertexBuffer.h"
#include "Rendering/Core/ConstantBuffer.h"
#include "Rendering/Core/BlendState.h"
#include "Application/SettingsAndDebug/ProfileScope.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/ShadowMapRenderer.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Gameplay/Stage/Component/Terrain.h"

#include <cfloat>
#include <cmath>

ShadowMapRenderer::ShadowMapRenderer(ID3D11Device* device, UINT shadowMapSize)
    : shadowMapSize(shadowMapSize)
{
    HRESULT hr;

    // ----- デプスオンリーテクスチャの生成 -----
    // デプスのみ作成してSRVでも使う
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

        // SRV（PBRShaderのslot8にバインドする）
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

    vertexShader = std::make_unique<Gpu::VertexShader>(device, "Resources/Shader/ShadowMapCasterVS.cso",
		ModelShader::InputLayout);

    // 6
    skeletonConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbSkeleton));

    // 7
    shadowSceneConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbShadowScene));

    CreateRasterizerStates(device);

    // ----- デプスステンシルステート -----
    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = true;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        depthStencilState = std::make_unique<Gpu::DepthStencilState>(device, desc);
    }
}

void ShadowMapRenderer::Draw(VMDLModel* model)
{
	if (model) drawList.push_back({model, &model->GetMeshes()});
}

void ShadowMapRenderer::Draw(
	VMDLModel* skeleton,
	const std::vector<VMDLModel::Mesh>* meshes)
{
	if (skeleton && meshes) drawList.push_back({skeleton, meshes});
}

void ShadowMapRenderer::Draw(Terrain* terrain)
{
    terrainDrawList.emplace_back(terrain);
}

void ShadowMapRenderer::Render(const RenderContext& rc,
                               const Vector3& lightDir,
                               float shadowDistance)
{
    PROFILE_FUNCTION();
    ID3D11DeviceContext* dc = rc.deviceContext;
    if (!rc.camera)
    {
        drawList.clear();
        terrainDrawList.clear();
        return;
    }

    const auto previousOutput = RenderTarget::CaptureOutput(dc);

    Gpu::PixelShader::UnbindShaderResources(dc, 8, CascadeCount);

    CalculateCascadeMatrices(*rc.camera, lightDir, shadowDistance);

    for (int cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        RenderCascade(dc, cascadeIndex);
    }

    drawList.clear();
    terrainDrawList.clear();

    RenderTarget::BindDepthTarget(dc, nullptr);

    Gpu::ConstantBuffer::UnbindVS(dc, 6, 2);
    RenderTarget::RestoreOutput(dc, previousOutput);
}

void ShadowMapRenderer::CreateRasterizerStates(ID3D11Device* device)
{
	D3D11_RASTERIZER_DESC desc{};
	desc.FillMode = D3D11_FILL_SOLID;
	desc.CullMode = D3D11_CULL_BACK;
	desc.DepthBias = depthBias;
	desc.SlopeScaledDepthBias = slopeScaledDepthBias;
	desc.DepthClipEnable = true;
	auto modelState = std::make_unique<Gpu::RasterizerState>(device, desc);
	desc.CullMode = D3D11_CULL_NONE;
	auto terrainState = std::make_unique<Gpu::RasterizerState>(device, desc);
	rasterizerState = std::move(modelState);
	terrainRasterizerState = std::move(terrainState);
}

void ShadowMapRenderer::DrawGUI()
{
	bool changed = ImGui::DragInt((const char*)u8"固定深度バイアス", &depthBias, 1.0f);
	changed |= ImGui::DragFloat((const char*)u8"傾斜バイアス係数", &slopeScaledDepthBias, 0.001f, 0.0f, 0.0f, "%.6f");
	ImGui::DragFloat((const char*)u8"深度比較バイアス", &comparisonBias, 0.00001f, 0.0f, 0.0f, "%.6f");
	const char* cullLabels[] = {(const char*)u8"両面を描画", (const char*)u8"裏面を描画（従来方式）", (const char*)u8"表面を描画"};
	if (changed) CreateRasterizerStates(Game::Graphics::Instance().GetDevice());
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
    const float lightLengthSquared = normalizedLightDir.LengthSquared();
    if (!std::isfinite(lightLengthSquared) || lightLengthSquared <= 0.000001f)
        normalizedLightDir = Vector3::UnitZ;
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

        //const float depthPadding = radius;
        const float depthPadding =
            std::max(radius * 2.0f, 200.0f);
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
    PROFILE_FUNCTION();
    CbShadowScene cbShadowScene{};
    cbShadowScene.lightViewProjection = lightViewProjections[cascadeIndex];
    shadowSceneConstantBuffer->Update(dc, &cbShadowScene);

    // ----- RenderTarget切り替え -----
    // カラーRTはnull（デプスのみ書き込み）
    // 呼出元が描画先を戻す
    {

        RenderTarget::BindDepthTarget(dc, depthDsvs[cascadeIndex].Get());
        RenderTarget::SetViewport(dc, shadowViewport);
        RenderTarget::ClearDepth(dc, depthDsvs[cascadeIndex].Get());
    }

    // ----- シェーダー＆ステート設定 -----
    vertexShader->Bind(dc);
    Gpu::PixelShader::Unbind(dc); // デプスのみ
    Gpu::BlendState::Unbind(dc); // ブレンド無効
    depthStencilState->Bind(dc);
    rasterizerState->Bind(dc);

    // 定数バッファ設定
    // VS slot6: スケルトン（ModelRendererと同じスロット番号で統一）
    // VS slot7: ライトViewProjection
    Gpu::ConstantBuffer::BindVS(dc, 6, {skeletonConstantBuffer.get(), shadowSceneConstantBuffer.get()});

    // ----- メッシュ描画 -----
	for (const ModelDrawInfo& drawInfo : drawList)
    {
		const Matrix renderScaleTransform = drawInfo.skeleton->GetRenderScaleTransform();
		for (const VMDLModel::Mesh& mesh : *drawInfo.meshes)
        {
            if (!mesh.isDraw || !mesh.vertexBuffer || !mesh.indexBuffer || mesh.indexCount == 0)
                continue;

            // スケルトン定数バッファ更新
            CbSkeleton cbSkeleton{};
            if (mesh.bones.size() > 0)
            {
                for (size_t i = 0; i < mesh.bones.size() && i < _countof(cbSkeleton.boneTransforms); ++i)
                {
                    const VMDLModel::Bone& bone = mesh.bones.at(i);
                    cbSkeleton.boneTransforms[i] =
                        bone.offsetTransform * bone.node->worldTransform * renderScaleTransform;
                }
            }
            else
            {
                cbSkeleton.boneTransforms[0] = mesh.node->worldTransform * renderScaleTransform;
            }
            skeletonConstantBuffer->Update(dc, &cbSkeleton);

            // 頂点バッファ・インデックスバッファ設定
            UINT stride = sizeof(VMDLModel::Vertex);
            UINT offset = 0;
            mesh.vertexBuffer->Bind(dc, stride, 0, offset);
            mesh.indexBuffer->Bind(dc, DXGI_FORMAT_R32_UINT, 0);
            Gpu::VertexBuffer::SetTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			mesh.indexBuffer->Draw(dc, mesh.indexCount);
        }
    }
    terrainRasterizerState->Bind(dc);

    for (Terrain* terrain : terrainDrawList)
    {
        if (terrain) terrain->RenderShadowMap(dc, lightViewProjections[cascadeIndex]);
    }

    // ----- 後始末 -----
    // SRVとしてPBRShaderで参照するため、DSVからは外しておく（同一リソースの同時バインド禁止）
    {

        RenderTarget::BindDepthTarget(dc, nullptr);
    }

}
