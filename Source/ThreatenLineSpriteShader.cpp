// ThreatenLineSpriteShader.cpp

#include "ThreatenLineSpriteShader.h"
#include "GpuResourceUtils.h"
#include "GameTime.h"

ThreatenLineSpriteShader::ThreatenLineSpriteShader(ID3D11Device* device)
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
		"Data/Shader/ThreatenLineSpriteShaderPS.cso",
		pixelShader.GetAddressOf());

	// ガウスフィルター用定数バッファ
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbThreatenLine),
		constantBuffer.GetAddressOf());
}

void ThreatenLineSpriteShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
}

void ThreatenLineSpriteShader::Update(
    const RenderContext& rc,
    ID3D11ShaderResourceView* srv,
    Vector2 textureSize,
    const ShaderParamList& params)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    CbThreatenLine data{};

    data.color = GetParam<Color>(
        params,
        "color",
        Color(1, 1, 1, 0.35f));

    data.center = GetParam<Vector2>(
        params,
        "center",
        Vector2(0.5f, 0.5f));

    data.screenAspect = GetParam<float>(
        params,
        "screenAspect",
        1280.0f / 720.0f);

    data.lineCount = GetParam<float>(
        params,
        "lineCount",
        64.0f);

    data.lineWidth = GetParam<float>(
        params,
        "lineWidth",
        0.14f);

    data.softness = GetParam<float>(
        params,
        "softness",
        0.08f);

    data.innerRadius = GetParam<float>(
        params,
        "innerRadius",
        0.18f);

    data.outerRadius = GetParam<float>(
        params,
        "outerRadius",
        0.98f);

    data.randomStrength = GetParam<float>(
        params,
        "randomStrength",
        0.45f);

    data.randomSeed = GetParam<float>(
        params,
        "randomSeed",
        0.0f);

    data.rotation = GetParam<float>(
        params,
        "rotation",
        0.0f);

    data.alphaMultiplier = GetParam<float>(
        params,
        "alphaMultiplier",
        1.0f);

    // ランダム化

    data.time = GetParam<float>(
        params,
        "time",
        0.0f);

    data.randomChangeSpeed = GetParam<float>(
        params,
        "randomChangeSpeed",
        12.0f);

    data.rotationSpeed = GetParam<float>(
        params,
        "rotationSpeed",
        0.0f);

    data.noiseScroll = GetParam<float>(
        params,
        "noiseScroll",
        0.0f);

    data.lineCount = max(data.lineCount, 1.0f);
    data.lineWidth = std::clamp(data.lineWidth, 0.001f, 1.0f);
    data.softness = std::clamp(data.softness, 0.001f, 1.0f);
    data.innerRadius = std::clamp(data.innerRadius, 0.0f, 2.0f);
    data.outerRadius = std::clamp(data.outerRadius, data.innerRadius + 0.001f, 3.0f);
    data.randomStrength = std::clamp(data.randomStrength, 0.0f, 1.0f);
    data.alphaMultiplier = std::clamp(data.alphaMultiplier, 0.0f, 1.0f);

    dc->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &data, 0, 0);

    ID3D11Buffer* cbs[] =
    {
        constantBuffer.Get()
    };
    dc->PSSetConstantBuffers(2, _countof(cbs), cbs);

    dc->PSSetShaderResources(0, 1, &srv);
}

void ThreatenLineSpriteShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	dc->VSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);

	ID3D11ShaderResourceView* nullSrv = nullptr;
	dc->PSSetShaderResources(0, 1, &nullSrv);

	ID3D11Buffer* nullCb = nullptr;
	dc->PSSetConstantBuffers(2, 1, &nullCb);
}
