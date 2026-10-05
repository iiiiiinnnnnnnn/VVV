// ModelRenderer.cpp
#include <algorithm>
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Resource/GpuResourceUtils.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Resource/MeshCache.h"
#include "Rendering/Renderer/ModelRenderer.h"
#include "Rendering/Shader/VMatShader.h"

// コンストラクタ
ModelRenderer::ModelRenderer(ID3D11Device* device)
{
	// シーン用定数バッファ
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbScene),
		sceneConstantBuffer.GetAddressOf());

	// スケルトン用定数バッファ
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbSkeleton),
		skeletonConstantBuffer.GetAddressOf());

	shaders[static_cast<int>(ModelShaderId::VMat)] = std::make_unique<VMatShader>(device);
}

// 箱描画
void ModelRenderer::Draw(
	ModelShaderId shaderId,
	std::shared_ptr<VMDLModel> model,
	const VMatRenderParams* params)
{
	DrawInfo& drawInfo = drawInfos.emplace_back();
	drawInfo.shaderId = shaderId;
	drawInfo.model = model;
	drawInfo.params = params;
}

// MeshCacheを描画待ちへ追加
void ModelRenderer::DrawMeshCache(
	ModelShaderId shaderId,
	std::shared_ptr<MeshCache> meshCache,
	std::shared_ptr<VMDLModel> skeleton,
	const VMatRenderParams* params)
{
	if (!meshCache || !skeleton) return;
	DrawInfo& drawInfo = drawInfos.emplace_back();
	drawInfo.shaderId = shaderId;
	drawInfo.model = std::move(skeleton);
	drawInfo.meshCache = std::move(meshCache);
	drawInfo.params = params;
}

// 描画実行
void ModelRenderer::Render(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シーン用定数バッファ更新
	{
		CbScene cbScene{};
		Matrix V = rc.camera->GetView();
		Matrix P = rc.camera->GetProjection();
		cbScene.viewProjection = V * P;
		cbScene.viewPosition = rc.camera->GetEye();
		cbScene.lightData = rc.lightManager->ConvertToCb();
		cbScene.distanceFogColor = rc.renderSettings.distanceFogColor;
		cbScene.distanceFogParams = {
			rc.renderSettings.distanceFogStart,
			rc.renderSettings.distanceFogEnd,
			rc.renderSettings.distanceFogStrength,
			rc.renderSettings.distanceFogEnabled ? 1.0f : 0.0f};
		dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, 0, &cbScene, 0, 0);
	}

	// 定数バッファ設定
	ID3D11Buffer* vsConstantBuffers[] =
	{
		skeletonConstantBuffer.Get(),
		sceneConstantBuffer.Get(),
	};
	ID3D11Buffer* psConstantBuffers[] =
	{
		sceneConstantBuffer.Get(),
	};
	dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
	dc->GSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
	dc->PSSetConstantBuffers(7, _countof(psConstantBuffers), psConstantBuffers);

	// サンプラステート設定
	// s0 = LinearWrap  : IBL用
	// s1 = LinearClamp : シャドウマップ用
	// s2 = マテリアルテクスチャ用
	ID3D11SamplerState* samplerStates[] =
	{
		rc.renderState->GetSamplerState(SamplerState::LinearWrap),
		rc.renderState->GetSamplerState(SamplerState::LinearClamp),
		rc.renderState->GetSamplerState(rc.renderSettings.pointMaterialTextures
			? SamplerState::PointWrapBaseLevel : SamplerState::LinearWrap),
	};
	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);

	// レンダーステート設定
	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(rc.renderState->GetRasterizerState(
		rc.renderSettings.wireframe ? RasterizerState::WireCullNone : RasterizerState::SolidCullNone));

	// メッシュ描画関数
	auto drawMesh = [&](const VMDLModel::Mesh& mesh, const Matrix& renderScaleTransform,
		ModelShader* shader, const VMatRenderParams* params)
	{
		// 頂点バッファ設定
		UINT stride = sizeof(VMDLModel::Vertex);
		UINT offset = 0;
		dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
		dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// スケルトン用定数バッファ更新
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
		dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);

		// 更新
		shader->Update(rc, mesh, params);

		// 描画
		dc->DrawIndexed(mesh.indexCount, 0, 0);
	};

	// ブレンドステート設定
	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

	// 不透明描画処理
	for (DrawInfo& drawInfo : drawInfos)
	{
		ModelShader* shader = shaders[static_cast<int>(drawInfo.shaderId)].get();
		const Matrix renderScaleTransform = drawInfo.model->GetRenderScaleTransform();
		shader->Begin(rc);

		const auto& meshes = drawInfo.meshCache
			? drawInfo.meshCache->GetMeshes()
			: drawInfo.model->GetMeshes();
		for (const VMDLModel::Mesh& mesh : meshes)
		{
			// 描画しないメッシュはスキップ
			if (!mesh.isDraw || !mesh.vertexBuffer || !mesh.indexBuffer || mesh.indexCount == 0)
				continue;

			// 半透明メッシュ登録
			float w = mesh.material->baseColor.w;
			float transmission = mesh.material->transmission;
			if (drawInfo.params)
			{
				const auto it = drawInfo.params->materials.find(mesh.material->name);
				if (it != drawInfo.params->materials.end() && it->second.baseColor)
					w = it->second.baseColor->w;
				if (it != drawInfo.params->materials.end() && it->second.transmission)
					transmission = *it->second.transmission;
			}
			if ((drawInfo.params && drawInfo.params->justDodgeUnlit) ||
				mesh.material->alphaMode == VMDLModel::AlphaMode::Blend ||
				transmission > 0.0f || (w > 0.01f && w < 0.99f))
			{
				TransparencyDrawInfo& transparencyDrawInfo = transparencyDrawInfos.emplace_back();
				transparencyDrawInfo.refractive = transmission > 0.0f;
				transparencyDrawInfo.mesh = &mesh;
				transparencyDrawInfo.shaderId = drawInfo.shaderId;
				transparencyDrawInfo.renderScaleTransform = renderScaleTransform;
				transparencyDrawInfo.params = drawInfo.params;
				// カメラとの距離を算出
				const Vector3 Position = (mesh.node->worldTransform * renderScaleTransform).Translation();
				DirectX::XMVECTOR Vec = Position - rc.camera->GetEye();
				transparencyDrawInfo.distance = rc.camera->GetFront().Dot(Vec);

				continue;
			}

			// 描画
			drawMesh(mesh, renderScaleTransform, shader, drawInfo.params);
		}

		shader->End(rc);
	}
	drawInfos.clear();

	// Copy the opaque scene before rendering refractive surfaces; never sample the active RTV.
	ID3D11ShaderResourceView* nullBackground = nullptr;
	dc->PSSetShaderResources(5, 1, &nullBackground);
	const bool needsTransmission = std::any_of(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
		[](const TransparencyDrawInfo& info) { return info.refractive; });
	bool backgroundAvailable = false;
	if (needsTransmission)
	{
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
		dc->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> source;
		if (target) target->GetResource(resource.GetAddressOf());
		if (resource) resource.As(&source);
		if (source)
		{
			D3D11_TEXTURE2D_DESC desc{}, previous{};
			source->GetDesc(&desc);
			if (transmissionBackground) transmissionBackground->GetDesc(&previous);
			if (desc.SampleDesc.Count == 1 && desc.ArraySize == 1)
			{
				if (!transmissionBackground || !transmissionBackgroundView || desc.Width != previous.Width ||
					desc.Height != previous.Height || desc.Format != previous.Format || desc.MipLevels != previous.MipLevels)
				{
					transmissionBackgroundView.Reset();
					transmissionBackground.Reset();
					desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
					desc.Usage = D3D11_USAGE_DEFAULT;
					desc.CPUAccessFlags = 0;
					desc.MiscFlags = 0;
					Microsoft::WRL::ComPtr<ID3D11Device> device;
					dc->GetDevice(device.GetAddressOf());
					if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, transmissionBackground.GetAddressOf())))
						device->CreateShaderResourceView(transmissionBackground.Get(), nullptr, transmissionBackgroundView.GetAddressOf());
				}
				if (transmissionBackgroundView)
				{
					dc->CopyResource(transmissionBackground.Get(), source.Get());
					backgroundAvailable = true;
				}
			}
		}
	}
	ID3D11ShaderResourceView* background = backgroundAvailable ? transmissionBackgroundView.Get() : nullptr;
	dc->PSSetShaderResources(5, 1, &background);

	// ブレンドステート設定
	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);

	// カメラから遠い順にソート
	std::sort(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
		[](const TransparencyDrawInfo& lhs, const TransparencyDrawInfo& rhs)
		{
			return lhs.distance > rhs.distance;
		});

	// 半透明描画処理
	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
	for (const TransparencyDrawInfo& transparencyDrawInfo : transparencyDrawInfos)
	{
		const bool refractive = transparencyDrawInfo.refractive;
		dc->RSSetState(rc.renderState->GetRasterizerState(rc.renderSettings.wireframe
			? RasterizerState::WireCullNone : refractive ? RasterizerState::SolidCullBack : RasterizerState::SolidCullNone));
		ModelShader* shader = shaders[static_cast<int>(transparencyDrawInfo.shaderId)].get();

		shader->Begin(rc);

		drawMesh(
			*transparencyDrawInfo.mesh,
			transparencyDrawInfo.renderScaleTransform,
			shader,
			transparencyDrawInfo.params);

		shader->End(rc);
	}
	transparencyDrawInfos.clear();
	dc->PSSetShaderResources(5, 1, &nullBackground);
	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(rc.renderState->GetRasterizerState(
		rc.renderSettings.wireframe ? RasterizerState::WireCullNone : RasterizerState::SolidCullNone));

	// 定数バッファ設定解除
	for (ID3D11Buffer*& vsConstantBuffer : vsConstantBuffers) { vsConstantBuffer = nullptr; }
	for (ID3D11Buffer*& psConstantBuffer : psConstantBuffers) { psConstantBuffer = nullptr; }
	dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
	dc->GSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
	dc->PSSetConstantBuffers(7, _countof(psConstantBuffers), psConstantBuffers);

	// サンプラステート設定解除
	for (ID3D11SamplerState*& samplerState : samplerStates) { samplerState = nullptr; }
	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);
}
