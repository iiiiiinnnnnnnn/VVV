#include "Misc.h"
#include "GpuResourceUtils.h"
#include "PBRShader.h"

PBRShader::PBRShader(ID3D11Device* device)
{
	// 頂点シェーダー
	GpuResourceUtils::LoadVertexShader(
		device,
		"Data/Shader/PBR_VS.cso",
		ModelShader::InputElementDescs.data(),
		static_cast<UINT>(ModelShader::InputElementDescs.size()),
		inputLayout.GetAddressOf(),
		vertexShader.GetAddressOf());

	// ピクセルシェーダー
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/PBR_PS.cso",
		pixelShader.GetAddressOf());

	// 定数バッファ作成
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbShadowMap),
		shadowMapBuffer.GetAddressOf());

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbPBR),
		pbrBuffer.GetAddressOf());
}

// 開始処理
void PBRShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダー設定
	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
}

// 更新処理
void PBRShader::Update(const RenderContext& rc, const Model::Mesh& mesh, float elapsedTime)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	CbPBR cbPBR;
	cbPBR.materialColor = mesh.material->baseColor;
	cbPBR.adjustMetalness = pbrData.metalness;
	cbPBR.adjustRoughness = pbrData.roughness;
	dc->UpdateSubresource(pbrBuffer.Get(), 0, 0, &cbPBR, 0, 0);

	CbShadowMap cbShadow;
	cbShadow.lightViewProjection = Matrix::Identity; // TODO: シャドウマップの view-projection 行列を設定
	cbShadow.shadowAttenuation = 0.5f;
	cbShadow.shadowBias = 0.0001f;
	dc->UpdateSubresource(shadowMapBuffer.Get(), 0, 0, &cbShadow, 0, 0);

	// 定数バッファ設定
	ID3D11Buffer* cbs[] =
	{
		pbrBuffer.Get(),
		shadowMapBuffer.Get()
	};
	dc->PSSetConstantBuffers(0, _countof(cbs), cbs);
	dc->VSSetConstantBuffers(0, _countof(cbs), cbs);

	ID3D11ShaderResourceView* srvs[] = {
		mesh.material->baseMap.Get(),
	};
	dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

void PBRShader::ApplyParams(ShaderParamPtr params)
{
	if (params)
	{
		pbrData = *static_cast<const PBRData*>(params);
	}
}

// 描画終了
void PBRShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダー設定解除
	dc->VSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);

	// 定数バッファ設定解除
	ID3D11Buffer* cbs[] = {nullptr, nullptr, nullptr};
	dc->PSSetConstantBuffers(8, _countof(cbs), cbs);

	// シェーダーリソースビュー設定解除
	ID3D11ShaderResourceView* srvs[] = {nullptr};
	dc->PSSetShaderResources(0, _countof(srvs), srvs);
}