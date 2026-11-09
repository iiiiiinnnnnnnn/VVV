#include <ProfileKit/ProfileKit.h>
#include "Resource/Texture.h"
#include "Rendering/Core/Graphics.h"
#include <Dx11Kit/Dx11Kit.h>
#include <algorithm>
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Resource/MeshCache.h"
#include "Rendering/Renderer/ModelRenderer.h"
#include "Rendering/Shader/PBRShader.h"

// コンストラクタ
ModelRenderer::ModelRenderer(ID3D11Device* device)
{
	// シーン用定数バッファ
	sceneConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbScene));

	// スケルトン用定数バッファ
	skeletonConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbSkeleton));

	auto shader = std::make_unique<PBRShader>(device);
	shaders[static_cast<int>(shader->GetId())] = std::move(shader);
}

ModelShader* ModelRenderer::GetShader(ModelShaderId id) const
{
	const int index = static_cast<int>(id);
	if (index < 0 || index >= static_cast<int>(ModelShaderId::EnumCount)) return nullptr;
	return shaders[index].get();
}

void ModelRenderer::Draw(std::shared_ptr<VMDLModel> model, const PBRRenderParams* params)
{
	Draw(ModelShaderId::PBR, std::move(model), params);
}

void ModelRenderer::DrawMeshCache(std::shared_ptr<MeshCache> meshCache,
	std::shared_ptr<VMDLModel> skeleton, const PBRRenderParams* params)
{
	DrawMeshCache(ModelShaderId::PBR, std::move(meshCache), std::move(skeleton), params);
}

// 箱描画
void ModelRenderer::Draw(
	ModelShaderId shaderId,
	std::shared_ptr<VMDLModel> model,
	const PBRRenderParams* params)
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
	const PBRRenderParams* params)
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
	PROFILE_FUNCTION();
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
		sceneConstantBuffer->Update(dc, &cbScene);
	}

	// 定数バッファ設定
	Gpu::ConstantBuffer::BindVS(dc, 6, {skeletonConstantBuffer.get(), sceneConstantBuffer.get()});
	Gpu::ConstantBuffer::BindGS(dc, 6, {skeletonConstantBuffer.get(), sceneConstantBuffer.get()});
	sceneConstantBuffer->BindPS(dc, 7);

	// メッシュ描画関数
	auto drawMesh = [&](const VMDLModel::Mesh& mesh, const Matrix& renderScaleTransform,
		ModelShader* shader, const PBRRenderParams* params)
	{
		// 頂点バッファ設定
		UINT stride = sizeof(VMDLModel::Vertex);
		UINT offset = 0;
		mesh.vertexBuffer->Bind(dc, stride, 0, offset);
		mesh.indexBuffer->Bind(dc, DXGI_FORMAT_R32_UINT, 0);
		Gpu::VertexBuffer::SetTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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
		skeletonConstantBuffer->Update(dc, &cbSkeleton);

		// 更新
		shader->Update(rc, mesh, params);

		// 描画
		mesh.indexBuffer->Draw(dc, mesh.indexCount);
		if (rc.renderSettings.shadingMode == ShadingMode::SolidAndWire)
		{
			const auto previousDepth = Gpu::DepthStencilState::Capture(dc);
			const auto previousRasterizer = Gpu::RasterizerState::Capture(dc);
			RenderContext wireContext = rc;
			wireContext.renderSettings.shadingMode = ShadingMode::Wire;
			shader->Update(wireContext, mesh, params);
			rc.graphics->DepthReadState->Bind(dc);
			rc.graphics->WireframeRasterizerState->Bind(dc);
			mesh.indexBuffer->Draw(dc, mesh.indexCount);
			Gpu::DepthStencilState::Restore(dc, previousDepth);
			Gpu::RasterizerState::Restore(dc, previousRasterizer.Get());
		}
	};

	// 不透明描画処理
	for (DrawInfo& drawInfo : drawInfos)
	{
		ModelShader* currentShader = nullptr;
		const Matrix renderScaleTransform = drawInfo.model->GetRenderScaleTransform();

		const auto& meshes = drawInfo.meshCache
			? drawInfo.meshCache->GetMeshes()
			: drawInfo.model->GetMeshes();
		for (const VMDLModel::Mesh& mesh : meshes)
		{
			// 描画しないメッシュはスキップ
			if (!mesh.isDraw || !mesh.vertexBuffer || !mesh.indexBuffer || mesh.indexCount == 0)
				continue;

			auto* shader = GetShader(mesh.material->shaderId);
			if (!shader) shader = GetShader(drawInfo.shaderId);
			if (!shader) continue;

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
			if (rc.renderSettings.shadingMode != ShadingMode::Shading) transmission = 0.0f;
			if ((drawInfo.params && drawInfo.params->justDodgeUnlit) ||
				mesh.material->alphaMode == VMDLModel::AlphaMode::Blend ||
				transmission > 0.0f || (w > 0.01f && w < 0.99f))
			{
				TransparencyDrawInfo& transparencyDrawInfo = transparencyDrawInfos.emplace_back();
				transparencyDrawInfo.refractive = transmission > 0.0f;
				transparencyDrawInfo.mesh = &mesh;
				transparencyDrawInfo.shaderId = shader->GetId();
				transparencyDrawInfo.renderScaleTransform = renderScaleTransform;
				transparencyDrawInfo.params = drawInfo.params;
				// カメラとの距離を算出
				const Vector3 Position = (mesh.node->worldTransform * renderScaleTransform).Translation();
				DirectX::XMVECTOR Vec = Position - rc.camera->GetEye();
				transparencyDrawInfo.distance = rc.camera->GetFront().Dot(Vec);

				continue;
			}

			if (shader != currentShader)
			{
				if (currentShader) currentShader->End(rc);
				shader->Begin(rc, DrawPass::Opaque);
				currentShader = shader;
			}

			// 描画
			drawMesh(mesh, renderScaleTransform, shader, drawInfo.params);
		}

		if (currentShader) currentShader->End(rc);
	}
	drawInfos.clear();

	// Copy the opaque scene before rendering refractive surfaces; never sample the active RTV.
	Gpu::PixelShader::UnbindShaderResources(dc, 5);
	const bool needsTransmission = std::any_of(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
		[](const TransparencyDrawInfo& info) { return info.refractive; });
	bool backgroundAvailable = false;
	if (needsTransmission)
	{
		auto target = RenderTarget::GetColorTarget(dc);
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
					auto device = Game::Graphics::Instance().GetDevice();
					if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, transmissionBackground.GetAddressOf())))
						device->CreateShaderResourceView(transmissionBackground.Get(), nullptr, transmissionBackgroundView.GetAddressOf());
				}
				if (transmissionBackgroundView)
				{
					Texture::Copy(dc, transmissionBackground.Get(), source.Get());
					backgroundAvailable = true;
				}
			}
		}
	}
	ID3D11ShaderResourceView* background = backgroundAvailable ? transmissionBackgroundView.Get() : nullptr;
	Gpu::PixelShader::SetShaderResources(dc, 5, background);

	// カメラから遠い順にソート
	std::sort(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
		[](const TransparencyDrawInfo& lhs, const TransparencyDrawInfo& rhs)
		{
			return lhs.distance > rhs.distance;
		});

	// 半透明描画処理
	for (const TransparencyDrawInfo& transparencyDrawInfo : transparencyDrawInfos)
	{
		ModelShader* shader = GetShader(transparencyDrawInfo.shaderId);
		if (!shader) continue;

		shader->Begin(rc, DrawPass::Transparent);

		drawMesh(
			*transparencyDrawInfo.mesh,
			transparencyDrawInfo.renderScaleTransform,
			shader,
			transparencyDrawInfo.params);

		shader->End(rc);
	}
	transparencyDrawInfos.clear();
	Gpu::PixelShader::UnbindShaderResources(dc, 5);

	// 定数バッファ設定解除
	Gpu::ConstantBuffer::UnbindVS(dc, 6, 2);
	Gpu::ConstantBuffer::UnbindGS(dc, 6, 2);
	Gpu::ConstantBuffer::UnbindPS(dc, 7, 1);
}
