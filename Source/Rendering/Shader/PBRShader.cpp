#include "Resource/ResourceManager.h"
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Color.h>
#include <Dx11Kit/Dx11Kit.h>
#include "Rendering/Core/Graphics.h"
#include "Rendering/Shader/PBRShader.h"
#include <imgui.h>

PBRShader::PBRShader(ID3D11Device* device)
{
	vertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/PBRVS.cso").c_str(),
		ModelShader::InputLayout);

	// ピクセルシェーダー
	pixelShader = std::make_unique<Gpu::PixelShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/PBRPS.cso").c_str());

	// 現在フラットシェーディングだけです。
	geometryShader = std::make_unique<Gpu::GeometryShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/PBRGS.cso").c_str());

	// シャドウマップ用定数バッファ (PSスロット0)
	shadowMapConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbShadowMap));

	// マテリアル用定数バッファ (PSスロット1)
	materialConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbMaterial));
}

// 描画開始
void PBRShader::Begin(const RenderContext& rc, DrawPass pass)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	const bool transparent = pass == DrawPass::Transparent;
	(transparent ? *rc.graphics->TransparentBlendState : *rc.graphics->OpaqueBlendState).Bind(dc);
	(transparent ? *rc.graphics->DepthReadState : *rc.graphics->DepthWriteState).Bind(dc);
	rc.graphics->SolidRasterizerState->Bind(dc);

	// s0: IBL、s1: シャドウ、s2: マテリアルテクスチャ
	Gpu::SamplerState::BindPS(dc, 0, {
		rc.graphics->LinearWrapSamplerState.get(), rc.graphics->LinearClampSamplerState.get(),
		rc.renderSettings.pointMaterialTextures ? rc.graphics->PointWrapBaseLevelSamplerState.get() : rc.graphics->LinearWrapSamplerState.get()});

	// シェーダー設定
	vertexShader->Bind(dc);
	Gpu::GeometryShader::Unbind(dc);
	pixelShader->Bind(dc);

	// SRVだけここでセット
	Gpu::PixelShader::SetShaderResources(dc, 8, rc.shadowMapData.shadowMaps);

	ID3D11ShaderResourceView* iblSrvs[] =
	{
		rc.iblData.ggxLookUpTableMap,
		rc.iblData.specularPremappingRadianceEnvironmentMap,
		rc.iblData.diffuseIrradianceEnvironmentMap,
	};
	Gpu::PixelShader::SetShaderResources(dc, 17, iblSrvs);
}

void PBRShader::Update(
	const RenderContext& rc,
	const VMDLModel::Mesh& mesh,
	const PBRRenderParams* params,
	const PBRMaterialParams* materialParams)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	pixelShader->Bind(dc);
	const float transmission = std::clamp(materialParams && materialParams->transmission
		? *materialParams->transmission : mesh.material->transmission, 0.0f, 1.0f);

	// シャドウCB更新
	{
		CbShadowMap cb{};
		for (int cascadeIndex = 0;
			cascadeIndex < ShadowMapData::CascadeCount;
			++cascadeIndex)
		{
			cb.lightViewProjections[cascadeIndex] =
				rc.shadowMapData.lightViewProjections[cascadeIndex];
		}
		cb.cascadeSplits = rc.shadowMapData.cascadeSplits;
		cb.cameraFront = Vector4(rc.camera->GetFront().x, rc.camera->GetFront().y,
			rc.camera->GetFront().z, 0.0f);
		cb.shadowColor = rc.shadowMapData.shadowColor;
		cb.shadowBias = rc.shadowMapData.shadowBias;
		cb.pcfKernelSize = rc.shadowMapData.pcfKernelSize;

		shadowMapConstantBuffer->Update(dc, &cb);
	}

	// マテリアルCB更新
	{
		CbMaterial cb{};
		cb.vivraDir = params ? params->vibrationDirection : Vector3::Zero;
		cb.vivraPower = params ? params->vibrationPower : 0.0f;
		cb.dissolveAmount = params ? std::clamp(params->dissolveAmount, 0.0f, 1.0f) : 0.0f;

		// Base Color
		cb.baseColor = materialParams && materialParams->baseColor
			? *materialParams->baseColor
			: mesh.material->baseColor;
		if (params) cb.baseColor *= params->baseColorTint;
		cb.useBaseColorTexture = materialParams && materialParams->useBaseColorTexture
			? (*materialParams->useBaseColorTexture ? 1 : 0)
			: (mesh.material->hasBaseTexture ? 1 : 0);

		// Metal Rough
		cb.metalness = std::clamp(
			materialParams && materialParams->metalness
				? *materialParams->metalness
				: mesh.material->metalness,
			0.0f,
			1.0f);
		cb.useMetalnessTexture = mesh.material->hasMetalnessTexture && !(materialParams && materialParams->metalness) ? 1 : 0;
		cb.roughness = std::clamp(
			materialParams && materialParams->roughness
				? *materialParams->roughness
				: mesh.material->roughness,
			0.0001f,
			1.0f);
		cb.useRoughnessTexture = mesh.material->hasRoughnessTexture && !(materialParams && materialParams->roughness) ? 1 : 0;

		// Occlusion
		const bool hasOcclusionTexture = mesh.material->hasOcclusionTexture;
		cb.occlusion = std::clamp(
			materialParams && materialParams->occlusion
				? *materialParams->occlusion
				: mesh.material->occlusion,
			0.0f,
			1.0f);
		cb.occlusionStrength = std::clamp(
			materialParams && materialParams->occlusionStrength
				? *materialParams->occlusionStrength
				: mesh.material->occlusionStrength,
			0.0f,
			1.0f);
		cb.useOcclusionTexture = hasOcclusionTexture && !(materialParams && materialParams->occlusion) ? 1 : 0;

		// Emissive
		const bool hasEmissiveTexture = mesh.material->hasEmissiveTexture;
		cb.emissiveColor = mesh.material->emissiveColor;
		cb.emissionColor = materialParams && materialParams->emissionColor
			? *materialParams->emissionColor
			: Color(0, 0, 0, 0);
		cb.useEmissiveTexture = hasEmissiveTexture ? 1 : 0;

		// Fresnel
		cb.fresnelColor = materialParams && materialParams->fresnelColor
			? *materialParams->fresnelColor
			: mesh.material->fresnelColor;
		cb.fresnelPower = std::max(
			materialParams && materialParams->fresnelPower
				? *materialParams->fresnelPower
				: mesh.material->fresnelPower,
			0.0001f);
		cb.fresnelStrength = std::max(
			materialParams && materialParams->fresnelStrength
				? *materialParams->fresnelStrength
				: mesh.material->fresnelStrength,
			0.0f);

		// 1.0 = 通常の影  0.0 = 影がかなり付きにくい
		cb.shadowStrength = std::clamp(
			materialParams && materialParams->shadowStrength
				? *materialParams->shadowStrength
				: mesh.material->shadowStrength,
			0.0f,
			1.0f);

		// FlatShading
		const bool isFlatShading = materialParams && materialParams->isFlatShading
			? *materialParams->isFlatShading
			: mesh.material->isFlatShading != 0;
		cb.isFlatShading = isFlatShading ? 1 : 0;

		// 定数バッファ更新
		cb.transmission = transmission;
		cb.indexOfRefraction = std::clamp(mesh.material->indexOfRefraction, 1.0f, 2.5f);
		cb.refractionDistance = std::clamp(mesh.material->refractionDistance, 0.0f, 5.0f);
		if (cb.transmission > 0.0f)
		{
			cb.metalness = 0.0f;
			cb.roughness = std::clamp(mesh.material->crystalRoughness, 0.0001f, 1.0f);
			cb.useMetalnessTexture = 0;
			cb.useRoughnessTexture = 0;
			cb.occlusionStrength = 0.0f;
			cb.fresnelStrength = 0.0f;
			cb.isFlatShading = 1;
		}
		materialConstantBuffer->Update(dc, &cb);
	}

	// フラットシェーディング(vmdl内でreflection使うと強制的にonになります)
	const bool isFlatShading = materialParams && materialParams->isFlatShading
		? *materialParams->isFlatShading
		: mesh.material->isFlatShading != 0;
	const bool useGeometryShader = isFlatShading || transmission > 0.0f;

	// CBセット
	Gpu::ConstantBuffer::BindPS(dc, 0, {shadowMapConstantBuffer.get(), materialConstantBuffer.get()});
	Gpu::ConstantBuffer::BindVS(dc, 0, {shadowMapConstantBuffer.get(), materialConstantBuffer.get()});
	if (useGeometryShader) geometryShader->Bind(dc);
	else Gpu::GeometryShader::Unbind(dc);
	Gpu::ConstantBuffer::BindGS(dc, 0, {shadowMapConstantBuffer.get(), materialConstantBuffer.get()});

	// マテリアルSRV
	ID3D11ShaderResourceView* srvs[] =
	{
		mesh.material->baseMap.Get(),
		mesh.material->normalMap.Get(),
		mesh.material->metalnessMap.Get(),
		mesh.material->occlusionMap.Get(),
		mesh.material->emissiveMap.Get(),
	};

	Gpu::PixelShader::SetShaderResources(dc, 0, srvs);
	ID3D11ShaderResourceView* scalarMaps[] = {mesh.material->roughnessMap.Get()};
	Gpu::PixelShader::SetShaderResources(dc, 6, scalarMaps);
}

// 描画終了
void PBRShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダー解除
	Gpu::VertexShader::Unbind(dc);
	Gpu::GeometryShader::Unbind(dc);
	Gpu::PixelShader::Unbind(dc);
	Gpu::VertexShader::UnbindInputLayout(dc);

	// 定数バッファ解除
	Gpu::ConstantBuffer::UnbindPS(dc, 0, 4);
	Gpu::ConstantBuffer::UnbindVS(dc, 0, 4);
	Gpu::ConstantBuffer::UnbindGS(dc, 0, 4);

	// SRV解除
	// slot 0?4: マテリアルテクスチャ
	// slot 8   : シャドウマップ
	// slot 17?19: IBL
	Gpu::PixelShader::UnbindShaderResources(dc, 0, 5);
	Gpu::PixelShader::UnbindShaderResources(dc, 6);

	Gpu::PixelShader::UnbindShaderResources(dc, 8, ShadowMapData::CascadeCount);

	Gpu::PixelShader::UnbindShaderResources(dc, 17, 3);
	Gpu::SamplerState::UnbindPS(dc, 0, 3);

	// 次の描画に向けて、深度書き込みと通常のカリングへ戻す。
	rc.graphics->DepthWriteState->Bind(dc);
	rc.graphics->SolidRasterizerState->Bind(dc);
}

bool PBRShader::DrawGUI(VMDLModel::Material& material)
{
	bool changed = false;
	// PBRパラメーター
	ImGuiToolKit::SeparatorText(u8"PBRシェーダー");
	changed |= ImGuiToolKit::ColorEdit4(u8"基本色", &material.baseColor.x);
	changed |= ImGuiToolKit::ColorEdit4(u8"発光色", &material.emissiveColor.x,
		ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
	ImGui::SetNextItemWidth(240.0f);
	changed |= ImGuiToolKit::SliderFloat(u8"メタリック", &material.metalness, 0.0f, 1.0f);
	ImGui::SetNextItemWidth(240.0f);
	changed |= ImGuiToolKit::SliderFloat(u8"ラフネス", &material.roughness, 0.0001f, 1.0f);
	ImGui::SetNextItemWidth(240.0f);
	changed |= ImGuiToolKit::SliderFloat(u8"オクルージョン", &material.occlusion, 0.0f, 1.0f);
	ImGui::SetNextItemWidth(240.0f);
	changed |= ImGuiToolKit::SliderFloat(u8"オクルージョン強度", &material.occlusionStrength, 0.0f, 1.0f);
	ImGui::SetNextItemWidth(240.0f);
	changed |= ImGuiToolKit::SliderFloat(u8"影の強度", &material.shadowStrength, 0.0f, 1.0f);

	ImGuiToolKit::SeparatorText(u8"リフレクション");
	bool crystalEnabled = material.transmission > 0.0f;
	if (ImGuiToolKit::Checkbox(u8"リフレクション", &crystalEnabled))
	{
		material.transmission = crystalEnabled ? 0.88f : 0.0f;
		changed = true;
	}
	if (crystalEnabled)
	{
		changed |= ImGuiToolKit::SliderFloat(u8"透過率", &material.transmission, 0.001f, 1.0f);
		changed |= ImGuiToolKit::SliderFloat(u8"屈折率", &material.indexOfRefraction, 1.0f, 2.5f);
		changed |= ImGuiToolKit::DragFloat(u8"屈折の距離", &material.refractionDistance,
			0.005f, 0.0f, 5.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		changed |= ImGuiToolKit::SliderFloat(u8"リフレクションのラフネス", &material.crystalRoughness, 0.0001f, 1.0f);
	}

	const char* alphaModes[] = {
		JText(u8"不透明"), JText(u8"マスク"), JText(u8"ブレンド")};
	int alphaMode = static_cast<int>(material.alphaMode);
	ImGui::SetNextItemWidth(240.0f);
	if (ImGuiToolKit::Combo(u8"アルファモード", &alphaMode, alphaModes, IM_ARRAYSIZE(alphaModes)))
	{
		material.alphaMode = static_cast<VMDLModel::AlphaMode>(alphaMode);
		changed = true;
	}
	if (material.alphaMode == VMDLModel::AlphaMode::Mask)
		changed |= ImGuiToolKit::SliderFloat(u8"アルファしきい値", &material.alphaCutoff, 0.0f, 1.0f);

	return changed;
}

bool PBRShader::DrawGUI(VMDLModel::VmdlMaterialKeyframe& key)
{
	bool changed = false;
	changed |= ImGuiToolKit::ColorEdit4(u8"基本色", &key.baseColor.x);
	changed |= ImGuiToolKit::ColorEdit4(u8"発光色", &key.emissiveColor.x,
		ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
	changed |= ImGuiToolKit::SliderFloat(u8"メタリック", &key.metalness, 0.0f, 1.0f);
	changed |= ImGuiToolKit::SliderFloat(u8"ラフネス", &key.roughness, 0.0001f, 1.0f);
	return changed;
}
