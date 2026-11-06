#include "Rendering/Core/PixelShader.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Core/VertexShader.h"
#include "Rendering/Shader/VignetteSpriteShader.h"


VignetteSpriteShader::VignetteSpriteShader(ID3D11Device* device, bool overlayOnly)
	: overlayOnly(overlayOnly)
{
	vertexShader = std::make_unique<Gpu::VertexShader>(device, "Resources/Shader/BasicSpriteVS.cso",
		SpriteShader::InputLayout);
	pixelShader = std::make_unique<Gpu::PixelShader>(device, "Resources/Shader/VignetteSpritePS.cso");
	constantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbVignette));
}

void VignetteSpriteShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	vertexShader->Bind(dc);
	pixelShader->Bind(dc);
	rc.graphics->LinearClampSamplerState->BindPS(dc, 0);
}

void VignetteSpriteShader::Update(const RenderContext& rc,
	ID3D11ShaderResourceView* srv, Vector2 textureSize, const Color& color,
	const SpriteRenderParams* params)
{
	CbVignette constant{};
	constant.color = color;
	constant.textureSize = textureSize;
	const SpriteVignetteParams* vignette = params ? &params->vignette : nullptr;
	constant.range = std::clamp(vignette ? vignette->range : 0.92f, 0.001f, 1.0f);
	constant.softness = std::clamp(
		vignette ? vignette->softness : 0.92f, 0.001f, constant.range);
	constant.overlayOnly = overlayOnly ? 1.0f : 0.0f;
	constantBuffer->Update(rc.deviceContext, &constant);
	constantBuffer->BindPS(rc.deviceContext, 2);
	Gpu::PixelShader::SetShaderResources(rc.deviceContext, 0, srv);
}

void VignetteSpriteShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	Gpu::VertexShader::Unbind(dc);
	Gpu::PixelShader::Unbind(dc);
	Gpu::VertexShader::UnbindInputLayout(dc);
	Gpu::PixelShader::UnbindShaderResources(dc, 0);
	Gpu::SamplerState::UnbindPS(dc, 0);
}
