#include "Rendering/Core/PixelShader.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Core/VertexShader.h"
#include "Rendering/Shader/BasicSpriteShader.h"

BasicSpriteShader::BasicSpriteShader(ID3D11Device* device)
{
	// 頂点シェーダー
	vertexShader = std::make_unique<Gpu::VertexShader>(device, "Resources/Shader/BasicSpriteVS.cso",
		SpriteShader::InputLayout);

	// ピクセルシェーダー
	pixelShader = std::make_unique<Gpu::PixelShader>(device, "Resources/Shader/BasicSpritePS.cso");

	// 定数バッファ
	constantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbBasic));
}

void BasicSpriteShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	vertexShader->Bind(dc);
	pixelShader->Bind(dc);
	rc.graphics->LinearClampSamplerState->BindPS(dc, 0);
}

void BasicSpriteShader::Update(
	const RenderContext& rc,
	ID3D11ShaderResourceView* srv,
	Vector2 textureSize,
	const Color& color,
	const SpriteRenderParams* params)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	CbBasic cb{};
	cb.color = color;

	//	定数バッファを設定
	constantBuffer->Update(dc, &cb);

	// 定数バッファ設定
	constantBuffer->BindPS(dc, 0);

	Gpu::PixelShader::SetShaderResources(dc, 0, srv);
}

void BasicSpriteShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	Gpu::VertexShader::Unbind(dc);
	Gpu::PixelShader::Unbind(dc);
	Gpu::VertexShader::UnbindInputLayout(dc);

	Gpu::PixelShader::UnbindShaderResources(dc, 0);
	Gpu::SamplerState::UnbindPS(dc, 0);
}
