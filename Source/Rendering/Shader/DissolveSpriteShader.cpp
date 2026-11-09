#include "Resource/ResourceManager.h"
#include <Dx11Kit/Dx11Kit.h>
#include "Rendering/Core/Graphics.h"
#include "Rendering/Shader/DissolveSpriteShader.h"
#include <algorithm>
#include "Resource/Texture.h"

DissolveSpriteShader::DissolveSpriteShader(ID3D11Device* device)
{
	vertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/BasicSpriteVS.cso").c_str(),
		SpriteShader::InputLayout);
	pixelShader = std::make_unique<Gpu::PixelShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/DissolveSpritePS.cso").c_str());
	constantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbDissolve));
}

void DissolveSpriteShader::Begin(const RenderContext& rc)
{
	auto dc = rc.deviceContext;
	vertexShader->Bind(dc);
	pixelShader->Bind(dc);
	rc.graphics->LinearClampSamplerState->BindPS(dc, 0);
}

void DissolveSpriteShader::Update(const RenderContext& rc, ID3D11ShaderResourceView* srv,
	Vector2 textureSize, const Color& color, const SpriteRenderParams* params)
{
	const auto mask = params ? params->dissolve.mask : nullptr;
	CbDissolve constant{};
	constant.color = color;
	constant.amount = mask ? std::clamp(params->dissolve.amount, 0.0f, 1.0f) : 0.0f;
	constant.maskScale = params ? std::clamp(params->dissolve.maskScale, 0.01f, 10.0f) : 1.0f;
	constant.softness = params ? std::clamp(params->dissolve.softness, 0.0f, 0.5f) : 0.0f;
	auto dc = rc.deviceContext;
	constantBuffer->Update(dc, &constant);
	constantBuffer->BindPS(dc, 0);
	ID3D11ShaderResourceView* resources[] = {srv, mask ? mask->GetShaderResourceView().Get() : nullptr};
	Gpu::PixelShader::SetShaderResources(dc, 0, resources);
}

void DissolveSpriteShader::End(const RenderContext& rc)
{
	auto dc = rc.deviceContext;
	Gpu::PixelShader::UnbindShaderResources(dc, 0, 2);
	rc.graphics->LinearClampSamplerState->BindPS(dc, 0);
	Gpu::VertexShader::Unbind(dc);
	Gpu::PixelShader::Unbind(dc);
	Gpu::VertexShader::UnbindInputLayout(dc);
}
