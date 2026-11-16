#include "Resource/ResourceManager.h"
#include <Dx11Kit/Dx11Kit.h>
#include "Rendering/Core/Graphics.h"
#include "Rendering/Shader/VignetteSpriteShader.h"


VignetteSpriteShader::VignetteSpriteShader(ID3D11Device* device)
{
	vertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/BasicSpriteVS.cso").c_str(),
		SpriteShader::InputLayout);
	pixelShader = std::make_unique<Gpu::PixelShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/VignettePS.cso").c_str());
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
	const SpriteVignetteParams* vignette = params ? &params->vignette : nullptr;
	const SpriteVignetteParams settings = vignette ? *vignette : SpriteVignetteParams{};
	constant.center = settings.center;
	constant.intensity = settings.intensity;
	constant.smoothness = settings.smoothness;
	constant.rounded = settings.rounded ? 1.0f : 0.0f;
	constant.roundness = settings.roundness;
	constant.tintColor = Color(1, 1, 1, 1);
	constant.spriteMode = 1.0f;
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
