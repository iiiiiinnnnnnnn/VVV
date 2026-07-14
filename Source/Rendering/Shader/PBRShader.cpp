// PBRShader.cpp

#include "Rendering/Shader/PBRShader.h"
#include "Resource/GpuResourceUtils.h"

PBRShader::PBRShader(ID3D11Device* device)
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
void PBRShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダーセットだけ
	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->GSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);

	// SRVだけここでセット
	dc->PSSetShaderResources(8, 1, &rc.shadowMapData.shadowMap);

	ID3D11ShaderResourceView* iblSrvs[] =
	{
		rc.iblData.ggxLookUpTableMap,
		rc.iblData.specularPremappingRadianceEnvironmentMap,
		rc.iblData.diffuseIrradianceEnvironmentMap,
	};
	dc->PSSetShaderResources(17, _countof(iblSrvs), iblSrvs);
}

void PBRShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シャドウCB更新
	{
		CbShadowMap cb{};
		cb.lightViewProjection = rc.shadowMapData.lightViewProjection;
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

		cb.baseColor = mesh.material->baseColor * GetParam<Color>(cachedParams, "color", {1.0f, 1.0f, 1.0f, 1.0f});
		cb.emissiveColor = mesh.material->emissiveColor;
		cb.emissionColor = GetParam<Color>(cachedParams, "emission", {0.0f, 0.0f, 0.0f, 0.0f});
		cb.fresnelColor = GetParam<Color>(cachedParams, "fresnelColor", {1.0f, 1.0f, 1.0f, 0.0f});
		cb.fresnelPower = GetParam<float>(cachedParams, "fresnelPower", 3.0f);
		cb.fresnelStrength = GetParam<float>(cachedParams, "fresnelStrength", 0.0f);

		const bool hasMetalnessParam = HasParam<float>(cachedParams, "metalness");
		const bool hasRoughnessParam = HasParam<float>(cachedParams, "roughness");
		const bool hasOcclusionParam = HasParam<float>(cachedParams, "occlusion");

		const bool hasMetalRoughTexture = mesh.material->metalnessRoughnessMap != nullptr;
		const bool hasOcclusionTexture = mesh.material->occlusionMap != nullptr;

		// ------------------------------------------------------------
		// metalness
		// パラメーターあり  : その値を使う
		// パラメーターなし + テクスチャあり : HLSL側でテクスチャを使う
		// どちらもなし : モデル側の値を使う
		// ------------------------------------------------------------
		if (hasMetalnessParam)
		{
			cb.metalness = GetParam<float>(cachedParams, "metalness", 0.0f);
			cb.useMetalnessTexture = 0;
		}
		else if (hasMetalRoughTexture)
		{
			cb.metalness = 0.0f;
			cb.useMetalnessTexture = 1;
		}
		else
		{
			cb.metalness = mesh.material->metalness;
			cb.useMetalnessTexture = 0;
		}

		// ------------------------------------------------------------
		// roughness
		// ------------------------------------------------------------
		if (hasRoughnessParam)
		{
			cb.roughness = GetParam<float>(cachedParams, "roughness", 0.5f);
			cb.useRoughnessTexture = 0;
		}
		else if (hasMetalRoughTexture)
		{
			cb.roughness = 0.5f;
			cb.useRoughnessTexture = 1;
		}
		else
		{
			cb.roughness = mesh.material->roughness;
			cb.useRoughnessTexture = 0;
		}

		// ------------------------------------------------------------
		// occlusion
		// occlusionStrengthは「AOそのもの」ではなく「AOの効き具合」
		// 手動でAO値を指定したい場合は "occlusion" を使う
		// ------------------------------------------------------------
		if (hasOcclusionParam)
		{
			cb.occlusion = GetParam<float>(cachedParams, "occlusion", 1.0f);
			cb.useOcclusionTexture = 0;
		}
		else if (hasOcclusionTexture)
		{
			cb.occlusion = 1.0f;
			cb.useOcclusionTexture = 1;
		}
		else
		{
			cb.occlusion = 1.0f;
			cb.useOcclusionTexture = 0;
		}

		cb.occlusionStrength = GetParam<float>(
			cachedParams,
			"occlusionStrength",
			mesh.material->occlusionStrength);

		// 1.0 = 通常の影
		// 0.0 = 影がかなり付きにくい
		// 肌は 0.5?0.75 くらいがおすすめ
		cb.shadowStrength = GetParam<float>(
			cachedParams,
			"shadowStrength",
			1.0f);

		const bool isFlatShading = GetParam<bool>(cachedParams, "IsFlatShading", false);
		cb.isFlatShading = isFlatShading ? 1 : 0;

		cb.metalness = std::clamp(cb.metalness, 0.0f, 1.0f);
		cb.roughness = std::clamp(cb.roughness, 0.0001f, 1.0f);
		cb.occlusion = std::clamp(cb.occlusion, 0.0f, 1.0f);
		cb.occlusionStrength = std::clamp(cb.occlusionStrength, 0.0f, 1.0f);
		cb.shadowStrength = std::clamp(cb.shadowStrength, 0.0f, 1.0f);
		cb.fresnelPower = (std::max)(cb.fresnelPower, 0.0001f);
		cb.fresnelStrength = (std::max)(cb.fresnelStrength, 0.0f);

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
	const bool hasDamageHoleParams = HasParam<int>(cachedParams, "holeCount");
	damageHoles.holeCount = std::clamp(GetParam<int>(cachedParams, "holeCount", 0), 0, MaxDamageHoles);
	damageHoles.edgeWidth = (std::max)(GetParam<float>(cachedParams, "holeEdgeWidth", 1.5f), 0.001f);
	damageHoles.depth = (std::max)(GetParam<float>(cachedParams, "holeDepth", 0.4f), 0.0f);
	const bool useDamageHoleGeometry = hasDamageHoleParams && damageHoles.depth > 0.0f;
	const bool useGeometryShader = useDamageHoleGeometry || GetParam<bool>(cachedParams, "IsFlatShading", false);

	for (int i = 0; i < damageHoles.holeCount; ++i)
	{
		const std::string name = "hole" + std::to_string(i);
		const std::string directionName = "holeDirection" + std::to_string(i);
		damageHoles.holes[i] = GetParam<Vector4>(cachedParams, name, Vector4(0, 0, 0, 0));
		damageHoles.holeDirections[i] =
			GetParam<Vector4>(cachedParams, directionName, Vector4(0, 0, 0, 0));
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
void PBRShader::End(const RenderContext& rc)
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

	ID3D11ShaderResourceView* nullSrv8 = nullptr;
	dc->PSSetShaderResources(8, 1, &nullSrv8);

	ID3D11ShaderResourceView* nullIblSrvs[3] = {};
	dc->PSSetShaderResources(17, _countof(nullIblSrvs), nullIblSrvs);
}



