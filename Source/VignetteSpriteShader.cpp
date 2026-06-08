// VignetteSpriteShader.cpp

#include "VignetteSpriteShader.h"
#include "GpuResourceUtils.h"

VignetteSpriteShader::VignetteSpriteShader(ID3D11Device* device)
{
	// 頂点シェーダー
	GpuResourceUtils::LoadVertexShader(
		device,
		"Data/Shader/BasicSpriteVS.cso",
		SpriteShader::InputElementDescs.data(),
		static_cast<UINT>(SpriteShader::InputElementDescs.size()),
		inputLayout.GetAddressOf(),
		vertexShader.GetAddressOf());

	// ピクセルシェーダー
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/VignetteSpritePS.cso",
		pixelShader.GetAddressOf());

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbVignette),
		constantBuffer.GetAddressOf());
}

void VignetteSpriteShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
}

void VignetteSpriteShader::Update(const RenderContext& rc, ID3D11ShaderResourceView* srv, Vector2 textureSize, const ShaderParamList& params)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	CbVignette constant = {};
	constant.color = GetParam<Vector4>(params, "color", Vector4(1, 0, 0, 1));
	dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &constant, 0, 0);

	//	定数バッファを設定
	dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &constant, 0, 0);

	// 定数バッファ設定
	ID3D11Buffer* cbs[] =
	{
		constantBuffer.Get()
	};
	dc->PSSetConstantBuffers(2, _countof(cbs), cbs);

	dc->PSSetShaderResources(0, 1, &srv);
}

void VignetteSpriteShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	dc->VSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);

	ID3D11ShaderResourceView* nullSrv = nullptr;
	dc->PSSetShaderResources(0, 1, &nullSrv);
}
