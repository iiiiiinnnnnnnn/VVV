// PBRShader.cpp

#include "PBRShader.h"
#include "GpuResourceUtils.h"

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
}

// 描画開始
void PBRShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダーセットだけ
	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
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
		dc->UpdateSubresource(shadowMapConstantBuffer.Get(), 0, 0, &cb, 0, 0);
	}

	// マテリアルCB更新
	{
		CbMaterial cb{};
		cb.baseColor = mesh.material->baseColor;
		cb.emissiveColor = mesh.material->emissiveColor;
		
		cb.metalness = GetParam<float>(cachedParams, "metalness", mesh.material->metalness);
		cb.roughness = GetParam<float>(cachedParams, "roughness", mesh.material->roughness);
		cb.occlusionStrength = GetParam<float>(cachedParams, "occlusionStrength", mesh.material->occlusionStrength);

		// テクスチャがある場合は、テクスチャの値を乗算
		if (mesh.material->metalnessRoughnessMap != nullptr)
		{
			cb.metalness *= mesh.material->metalness;
			cb.roughness *= mesh.material->roughness;
			cb.occlusionStrength *= mesh.material->occlusionStrength;
		}
		dc->UpdateSubresource(materialConstantBuffer.Get(), 0, 0, &cb, 0, 0);
	}

	// CBセット
	ID3D11Buffer* cbs[] = {shadowMapConstantBuffer.Get(), materialConstantBuffer.Get()};
	dc->PSSetConstantBuffers(0, _countof(cbs), cbs);
	dc->VSSetConstantBuffers(0, 1, shadowMapConstantBuffer.GetAddressOf());

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
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);

	// 定数バッファ解除
	ID3D11Buffer* nullCbs[] = {nullptr, nullptr};
	dc->PSSetConstantBuffers(0, _countof(nullCbs), nullCbs);
	dc->VSSetConstantBuffers(0, _countof(nullCbs), nullCbs);

	// SRV解除
	// slot 0〜4: マテリアルテクスチャ
	// slot 8   : シャドウマップ
	// slot 17〜19: IBL
	ID3D11ShaderResourceView* nullSrvs[5] = {};
	dc->PSSetShaderResources(0, _countof(nullSrvs), nullSrvs);

	ID3D11ShaderResourceView* nullSrv8 = nullptr;
	dc->PSSetShaderResources(8, 1, &nullSrv8);

	ID3D11ShaderResourceView* nullIblSrvs[3] = {};
	dc->PSSetShaderResources(17, _countof(nullIblSrvs), nullIblSrvs);
}
