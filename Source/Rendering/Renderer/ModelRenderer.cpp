// ModelRenderer.cpp

#include <algorithm>
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Resource/GpuResourceUtils.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Rendering/Renderer/ModelRenderer.h"
#include "Rendering/Shader/BasicModelShader.h"

// 追加シェーダー
#include "Rendering/Shader/PBRShader.h"

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

	shaders[static_cast<int>(ModelShaderId::Basic)] = std::make_unique<BasicModelShader>(device);

	// 追加シェーダー
	shaders[static_cast<int>(ModelShaderId::PBR)] = std::make_unique<PBRShader>(device);
}

// 箱描画
void ModelRenderer::Draw(ModelShaderId shaderId, std::shared_ptr<VMDLModel> model, std::unordered_map<std::string, ShaderParamList> paramsWithMaterial)
{
	DrawInfo& drawInfo = drawInfos.emplace_back();
	drawInfo.shaderId = shaderId;
	drawInfo.model = model;
	drawInfo.paramsWithMaterial = paramsWithMaterial;
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
	// s0 = LinearWrap  : マテリアルテクスチャ & IBL用
	// s1 = LinearClamp : シャドウマップ用
	ID3D11SamplerState* samplerStates[] =
	{
		rc.renderState->GetSamplerState(SamplerState::LinearWrap),
		rc.renderState->GetSamplerState(SamplerState::LinearClamp),
	};
	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);

	// レンダーステート設定
	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(rc.renderState->GetRasterizerState(
		rc.renderSettings.wireframe ? RasterizerState::WireCullNone : RasterizerState::SolidCullNone));

	// メッシュ描画関数
	auto drawMesh = [&](const VMDLModel::Mesh& mesh, ModelShader* shader, std::unordered_map<std::string, ShaderParamList> paramsWithMaterial)
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

		// マテリアル名でパラメータを引いてシェーダーに渡す
		auto it = paramsWithMaterial.find(mesh.material->name);
		const ShaderParamList& params = (it != paramsWithMaterial.end()) ? it->second : ShaderParamList{};
		shader->ApplyShaderParams(params);

		// 更新
		shader->Update(rc, mesh);

		// 描画
		dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
	};

	// ブレンドステート設定
	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

	// 不透明描画処理
	for (DrawInfo& drawInfo : drawInfos)
	{
		ModelShader* shader = shaders[static_cast<int>(drawInfo.shaderId)].get();
		shader->Begin(rc);

		for (const VMDLModel::Mesh& mesh : drawInfo.model->GetMeshes())
		{
			// 描画しないメッシュはスキップ
			if (!mesh.isDraw)
				continue;

			// 半透明メッシュ登録
			float shaderParamColorA = GetParam<Color>(drawInfo.paramsWithMaterial[mesh.material->name], "color", {1.0f, 1.0f, 1.0f, 1.0f}).w;
			if (mesh.material->alphaMode == VMDLModel::AlphaMode::Blend ||
				(mesh.material->baseColor.w > 0.01f && mesh.material->baseColor.w < 0.99f) ||
				(shaderParamColorA > 0.01f && shaderParamColorA < 0.99f))
			{
				TransparencyDrawInfo& transparencyDrawInfo = transparencyDrawInfos.emplace_back();
				transparencyDrawInfo.mesh = &mesh;
				transparencyDrawInfo.shaderId = drawInfo.shaderId;
				transparencyDrawInfo.paramsWithMaterial = drawInfo.paramsWithMaterial;
				// カメラとの距離を算出
				Vector3 Position = {mesh.node->worldTransform._41, mesh.node->worldTransform._42, mesh.node->worldTransform._43};
				DirectX::XMVECTOR Vec = Position - rc.camera->GetEye();
				transparencyDrawInfo.distance = rc.camera->GetFront().Dot(Vec);

				continue;
			}

			// 描画
			drawMesh(mesh, shader, drawInfo.paramsWithMaterial);
		}

		shader->End(rc);
	}
	drawInfos.clear();

	// ブレンドステート設定
	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);

	// カメラから遠い順にソート
	std::sort(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
		[](const TransparencyDrawInfo& lhs, const TransparencyDrawInfo& rhs)
		{
			return lhs.distance > rhs.distance;
		});

	// 半透明描画処理
	for (const TransparencyDrawInfo& transparencyDrawInfo : transparencyDrawInfos)
	{
		ModelShader* shader = shaders[static_cast<int>(transparencyDrawInfo.shaderId)].get();

		shader->Begin(rc);

		drawMesh(*transparencyDrawInfo.mesh, shader, transparencyDrawInfo.paramsWithMaterial);

		shader->End(rc);
	}
	transparencyDrawInfos.clear();

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

void ModelRenderer::SetShaderParamForAllMaterials(VMDLModel* model, const ShaderParam& param, ShaderParamListWithMaterialName& paramsWithMaterial)
{
	if (!model)
		return;

	for (const VMDLModel::Material& material : model->GetMaterials())
	{
		ShaderParamList& params = paramsWithMaterial[material.name];
		auto it = std::find_if(
			params.begin(),
			params.end(),
			[&](const ShaderParam& p) { return p.name == param.name; });

		if (it != params.end())
		{
			it->value = param.value;
		}
		else
		{
			params.push_back(param);
		}
	}
}

void ModelRenderer::SetShaderParamForAllMaterials(VMDLModel* model, const ShaderParamList& paramList, ShaderParamListWithMaterialName& paramsWithMaterial)
{
	if (!model)
		return;

	for (const VMDLModel::Material& material : model->GetMaterials())
	{
		ShaderParamList& params = paramsWithMaterial[material.name];

		for (const ShaderParam& param : paramList)
		{
			auto it = std::find_if(
				params.begin(),
				params.end(),
				[&](const ShaderParam& p) { return p.name == param.name; });

			if (it != params.end())
			{
				it->value = param.value;
			}
			else
			{
				params.push_back(param);
			}
		}
	}
}
