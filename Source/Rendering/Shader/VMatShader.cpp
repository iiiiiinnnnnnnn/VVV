// VMatShader.cpp

#include "Rendering/Shader/VMatShader.h"
#include "Resource/GpuResourceUtils.h"

VMatShader::VMatShader(ID3D11Device* device)
{
	GpuResourceUtils::LoadVertexShader(
		device,
		"Data/Shader/PBRVS.cso",
		ModelShader::InputElementDescs.data(),
		static_cast<UINT>(ModelShader::InputElementDescs.size()),
		inputLayout.GetAddressOf(),
		vertexShader.GetAddressOf());

	// ピクセルシェーダー
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/PBRPS.cso",
		pixelShader.GetAddressOf());

	GpuResourceUtils::LoadGeometryShader(
		device,
		"Data/Shader/PBRGS.cso",
		geometryShader.GetAddressOf());

	// シャドウマップ用定数バッファ (PSスロット0)
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbShadowMap),
		shadowMapConstantBuffer.GetAddressOf());

	// マテリアル用定数バッファ (PSスロット1)
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbMaterial),
		materialConstantBuffer.GetAddressOf());

	// ダメージ穴用定数バッファ (PSスロット2)
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbDamageHoles),
		damageHolesConstantBuffer.GetAddressOf());
}

// 描画開始
void VMatShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダーセットだけ
	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->GSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);

	// SRVだけここでセット
	dc->PSSetShaderResources(
		8,
		ShadowMapData::CascadeCount,
		rc.shadowMapData.shadowMaps.data());

	ID3D11ShaderResourceView* iblSrvs[] =
	{
		rc.iblData.ggxLookUpTableMap,
		rc.iblData.specularPremappingRadianceEnvironmentMap,
		rc.iblData.diffuseIrradianceEnvironmentMap,
	};
	dc->PSSetShaderResources(17, _countof(iblSrvs), iblSrvs);
}

void VMatShader::Update(
	const RenderContext& rc,
	const VMDLModel::Mesh& mesh,
	const VMatRenderParams* params)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	const VMatMaterialParams* materialParams = nullptr;
	if (params)
	{
		const auto it = params->materials.find(mesh.material->name);
		if (it != params->materials.end()) materialParams = &it->second;
	}

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

		dc->UpdateSubresource(
			shadowMapConstantBuffer.Get(),
			0,
			nullptr,
			&cb,
			0,
			0);
	}

	// マテリアルCB更新
	{
		CbMaterial cb{};

		// Base Color
		cb.baseColor = materialParams && materialParams->baseColor
			? *materialParams->baseColor
			: mesh.material->baseColor;
		cb.useBaseColorTexture = materialParams && materialParams->useBaseColorTexture
			? (*materialParams->useBaseColorTexture ? 1 : 0)
			: 1;

		// Metal Rough
		const bool hasMetalRoughTexture = mesh.material->metalnessRoughnessMap != nullptr;
		cb.metalness = std::clamp(
			materialParams && materialParams->metalness
				? *materialParams->metalness
				: mesh.material->metalness,
			0.0f,
			1.0f);
		cb.useMetalnessTexture = hasMetalRoughTexture && !(materialParams && materialParams->metalness) ? 1 : 0;
		cb.roughness = std::clamp(
			materialParams && materialParams->roughness
				? *materialParams->roughness
				: mesh.material->roughness,
			0.0001f,
			1.0f);
		cb.useRoughnessTexture = hasMetalRoughTexture && !(materialParams && materialParams->roughness) ? 1 : 0;

		// Occlusion
		const bool hasOcclusionTexture = mesh.material->occlusionMap != nullptr;
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
		const bool hasEmissiveTexture = mesh.material->emissiveMap != nullptr;
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
		dc->UpdateSubresource(
			materialConstantBuffer.Get(),
			0,
			nullptr,
			&cb,
			0,
			0);
	}

	// ダメージ穴CB更新
	CbDamageHoles damageHoles{};
	const VMatDamageHoleParams* holeParams = params ? &params->damageHoles : nullptr;
	damageHoles.count = holeParams
		? std::clamp(holeParams->count, 0, VMatDamageHoleParams::MaxCount)
		: 0;
	damageHoles.edgeWidth = holeParams ? std::max(holeParams->edgeWidth, 0.001f) : 1.5f;
	damageHoles.depth = holeParams ? std::max(holeParams->depth, 0.0f) : 0.4f;
	const bool useDamageHoleGeometry = damageHoles.count > 0 && damageHoles.depth > 0.0f;
	const bool isFlatShading = materialParams && materialParams->isFlatShading
		? *materialParams->isFlatShading
		: mesh.material->isFlatShading != 0;
	const bool useGeometryShader = useDamageHoleGeometry || isFlatShading;

	for (int i = 0; i < damageHoles.count; ++i)
	{
		damageHoles.holes[i] = holeParams->holes[i];
		damageHoles.directions[i] = holeParams->directions[i];
	}

	dc->UpdateSubresource(
		damageHolesConstantBuffer.Get(),
		0,
		nullptr,
		&damageHoles,
		0,
		0);

	// CBセット
	ID3D11Buffer* cbs[] =
	{
		shadowMapConstantBuffer.Get(),
		materialConstantBuffer.Get(),
		damageHolesConstantBuffer.Get()
	};

	dc->PSSetConstantBuffers(0, _countof(cbs), cbs);
	dc->VSSetConstantBuffers(0, 1, shadowMapConstantBuffer.GetAddressOf());
	dc->GSSetShader(useGeometryShader ? geometryShader.Get() : nullptr, nullptr, 0);
	dc->GSSetConstantBuffers(0, _countof(cbs), cbs);

	// マテリアルSRV
	ID3D11ShaderResourceView* srvs[] =
	{
		mesh.material->baseMap.Get(),
		mesh.material->normalMap.Get(),
		mesh.material->metalnessRoughnessMap.Get(),
		mesh.material->occlusionMap.Get(),
		mesh.material->emissiveMap.Get(),
	};

	dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

// 描画終了
void VMatShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダー解除
	dc->VSSetShader(nullptr, nullptr, 0);
	dc->GSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);

	// 定数バッファ解除
	ID3D11Buffer* nullCbs[] = { nullptr, nullptr, nullptr };
	dc->PSSetConstantBuffers(0, _countof(nullCbs), nullCbs);
	dc->VSSetConstantBuffers(0, _countof(nullCbs), nullCbs);
	dc->GSSetConstantBuffers(0, _countof(nullCbs), nullCbs);

	// SRV解除
	// slot 0?4: マテリアルテクスチャ
	// slot 8   : シャドウマップ
	// slot 17?19: IBL
	ID3D11ShaderResourceView* nullSrvs[5] = {};
	dc->PSSetShaderResources(0, _countof(nullSrvs), nullSrvs);

	ID3D11ShaderResourceView* nullShadowSrvs[ShadowMapData::CascadeCount] = {};
	dc->PSSetShaderResources(
		8,
		ShadowMapData::CascadeCount,
		nullShadowSrvs);

	ID3D11ShaderResourceView* nullIblSrvs[3] = {};
	dc->PSSetShaderResources(17, _countof(nullIblSrvs), nullIblSrvs);
}

