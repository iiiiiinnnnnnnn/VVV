// BasicSpriteShader.cpp

#include "Rendering/Shader/BasicSpriteShader.h"
#include "Resource/GpuResourceUtils.h"

BasicSpriteShader::BasicSpriteShader(ID3D11Device* device)
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
		"Data/Shader/BasicSpritePS.cso",
		pixelShader.GetAddressOf());

	// 定数バッファ
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbBasic),
		constantBuffer.GetAddressOf());
}

void BasicSpriteShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
}

void BasicSpriteShader::Update(
	const RenderContext& rc,
	ID3D11ShaderResourceView* srv,
	Vector2 textureSize,
	const Color& color)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	CbBasic cb{};
	cb.color = color;

	//	定数バッファを設定
	dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &cb, 0, 0);

	// 定数バッファ設定
	ID3D11Buffer* cbs[] =
	{
		constantBuffer.Get()
	};
	dc->PSSetConstantBuffers(0, _countof(cbs), cbs);

	dc->PSSetShaderResources(0, 1, &srv);
}

void BasicSpriteShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	dc->VSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);

	ID3D11ShaderResourceView* nullSrv = nullptr;
	dc->PSSetShaderResources(0, 1, &nullSrv);
}
