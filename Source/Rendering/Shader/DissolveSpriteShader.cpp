#include "Rendering/Shader/DissolveSpriteShader.h"
#include <algorithm>
#include "Resource/GpuResourceUtils.h"
#include "Resource/Texture.h"

DissolveSpriteShader::DissolveSpriteShader(ID3D11Device* device)
{
	GpuResourceUtils::LoadVertexShader(device, "Resources/Shader/BasicSpriteVS.cso",
		SpriteShader::InputElementDescs.data(),
		static_cast<UINT>(SpriteShader::InputElementDescs.size()),
		inputLayout.GetAddressOf(), vertexShader.GetAddressOf());
	GpuResourceUtils::LoadPixelShader(device, "Resources/Shader/DissolveSpritePS.cso",
		pixelShader.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbDissolve), constantBuffer.GetAddressOf());
}

void DissolveSpriteShader::Begin(const RenderContext& rc)
{
	auto dc = rc.deviceContext;
	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
	auto sampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
	dc->PSSetSamplers(0, 1, &sampler);
}

void DissolveSpriteShader::Update(const RenderContext& rc, ID3D11ShaderResourceView* srv,
	Vector2 textureSize, const Color& color, const SpriteRenderParams* params)
{
	const auto mask = params ? params->dissolve.mask : nullptr;
	CbDissolve constant{};
	constant.color = color;
	constant.amount = mask ? std::clamp(params->dissolve.amount, 0.0f, 1.0f) : 0.0f;
	auto dc = rc.deviceContext;
	dc->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &constant, 0, 0);
	ID3D11Buffer* buffer = constantBuffer.Get();
	dc->PSSetConstantBuffers(0, 1, &buffer);
	ID3D11ShaderResourceView* resources[] = {srv, mask ? mask->GetShaderResourceView().Get() : nullptr};
	dc->PSSetShaderResources(0, 2, resources);
}

void DissolveSpriteShader::End(const RenderContext& rc)
{
	auto dc = rc.deviceContext;
	ID3D11ShaderResourceView* resources[] = {nullptr, nullptr};
	dc->PSSetShaderResources(0, 2, resources);
	ID3D11SamplerState* sampler = nullptr;
	dc->PSSetSamplers(0, 1, &sampler);
	dc->VSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);
}
