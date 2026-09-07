#include "Rendering/Shader/VignetteSpriteShader.h"

#include "Resource/GpuResourceUtils.h"

VignetteSpriteShader::VignetteSpriteShader(ID3D11Device* device)
{
	GpuResourceUtils::LoadVertexShader(device, "Resources/Shader/BasicSpriteVS.cso",
		SpriteShader::InputElementDescs.data(),
		static_cast<UINT>(SpriteShader::InputElementDescs.size()),
		inputLayout.GetAddressOf(), vertexShader.GetAddressOf());
	GpuResourceUtils::LoadPixelShader(device, "Resources/Shader/VignetteSpritePS.cso",
		pixelShader.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(
		device, sizeof(CbVignette), constantBuffer.GetAddressOf());
}

void VignetteSpriteShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
	ID3D11SamplerState* sampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
	dc->PSSetSamplers(0, 1, &sampler);
}

void VignetteSpriteShader::Update(const RenderContext& rc,
	ID3D11ShaderResourceView* srv, Vector2 textureSize, const Color& color,
	const Vector4& parameters)
{
	CbVignette constant{};
	constant.color = color;
	constant.textureSize = textureSize;
	constant.range = std::clamp(parameters.x, 0.001f, 1.0f);
	constant.softness = std::clamp(parameters.y, 0.001f, constant.range);
	rc.deviceContext->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &constant, 0, 0);
	ID3D11Buffer* buffers[] = {constantBuffer.Get()};
	rc.deviceContext->PSSetConstantBuffers(2, _countof(buffers), buffers);
	rc.deviceContext->PSSetShaderResources(0, 1, &srv);
}

void VignetteSpriteShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	dc->VSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);
	ID3D11ShaderResourceView* nullSrv = nullptr;
	dc->PSSetShaderResources(0, 1, &nullSrv);
	ID3D11SamplerState* nullSampler = nullptr;
	dc->PSSetSamplers(0, 1, &nullSampler);
}
