// PostProccess.cpp

#include "PostProccess.h"

#include "GpuResourceUtils.h"
#include "Graphics.h"
#include "RenderTarget.h"
#include "Shader.h"
#include "imgui.h"

const std::vector<D3D11_INPUT_ELEMENT_DESC> PostProccess::InputElementDescs =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

PostProccess::PostProccess()
{
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();

	// Copy
	copyEffect = std::make_unique<DirectX::BasicPostProcess>(device);
	copyEffect->SetEffect(DirectX::BasicPostProcess::Copy);

	// Bloom
	bloomExtract = std::make_unique<DirectX::BasicPostProcess>(device);
	bloomExtract->SetEffect(DirectX::BasicPostProcess::BloomExtract);

	// Bloom Blur
	bloomBlur = std::make_unique<DirectX::BasicPostProcess>(device);
	bloomBlur->SetEffect(DirectX::BasicPostProcess::BloomBlur);

	// Basic Effect
	basicEffect = std::make_unique<DirectX::BasicPostProcess>(device);
	basicEffect->SetEffect(DirectX::BasicPostProcess::Copy);

	// Dual Effect
	bloomCombine = std::make_unique<DirectX::DualPostProcess>(device);
	bloomCombine->SetEffect(DirectX::DualPostProcess::BloomCombine);

	// Tone Mapping
	toneMap = std::make_unique<DirectX::ToneMapPostProcess>(device);
	toneMap->SetOperator(DirectX::ToneMapPostProcess::ACESFilmic);
	toneMap->SetTransferFunction(DirectX::ToneMapPostProcess::SRGB);

	// SSAO
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/SSAOPS.cso",
		SSAOPixelShader.GetAddressOf());
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/SSAOCompositePS.cso",
		SSAOCompositePixelShader.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbSSAO),
		SSAOConstantBuffer.GetAddressOf());

	// Radial Blur
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/RadialBlurPS.cso",
		radialBlurPixelShader.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbRadialBlur),
		radialBlurConstantBuffer.GetAddressOf());

	// Vignette
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/VignettePS.cso",
		vignettePixelShader.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbVignette),
		vignetteConstantBuffer.GetAddressOf());

	// Chromatic Aberration
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/ChromaticAberrationPS.cso",
		chromaticAberrationPixelShader.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbChromaticAberration),
		chromaticAberrationConstantBuffer.GetAddressOf());

	// Fullscreen quad
	GpuResourceUtils::LoadVertexShader(
		device,
		"Data/Shader/FullScreenQuadVS.cso",
		InputElementDescs.data(),
		static_cast<UINT>(InputElementDescs.size()),
		fullscreenInputLayout.GetAddressOf(),
		fullscreenVertexShader.GetAddressOf());
	const FullscreenVertex vertices[] =
	{
		{{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},
		{{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}},
		{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
		{{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
	};
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth = sizeof(vertices);
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = vertices;
	device->CreateBuffer(&vertexBufferDesc, &vertexData, fullscreenVertexBuffer.GetAddressOf());
}

void PostProccess::Copy(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
{
	copyEffect->SetEffect(DirectX::BasicPostProcess::Copy);
	copyEffect->SetSourceTexture(colorMap);
	copyEffect->Process(rc.deviceContext);
	UnbindShaderResources(rc.deviceContext);
}

void PostProccess::LuminanceExtraction(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
{
	bloomExtract->SetEffect(DirectX::BasicPostProcess::BloomExtract);
	bloomExtract->SetSourceTexture(colorMap);
	bloomExtract->SetBloomExtractParameter(bloomThreshold);
	bloomExtract->Process(rc.deviceContext);
	UnbindShaderResources(rc.deviceContext);
}

void PostProccess::BloomBlur(const RenderContext& rc, ID3D11ShaderResourceView* bloomMap, bool horizontal)
{
	bloomBlur->SetEffect(DirectX::BasicPostProcess::BloomBlur);
	bloomBlur->SetSourceTexture(bloomMap);
	bloomBlur->SetBloomBlurParameters(horizontal, bloomBlurSize, bloomBlurBrightness);
	bloomBlur->Process(rc.deviceContext);
	UnbindShaderResources(rc.deviceContext);
}

void PostProccess::Bloom(
	const RenderContext& rc,
	ID3D11ShaderResourceView* colorMap,
	ID3D11ShaderResourceView* bloomMap)
{
	const auto effect = static_cast<DirectX::DualPostProcess::Effect>(dualEffectIndex);
	bloomCombine->SetEffect(effect);
	bloomCombine->SetSourceTexture(colorMap);
	bloomCombine->SetSourceTexture2(bloomMap);
	if (effect == DirectX::DualPostProcess::Merge)
	{
		bloomCombine->SetMergeParameters(mergeWeight1, mergeWeight2);
	}
	else
	{
		bloomCombine->SetBloomCombineParameters(
			bloomIntensity,
			baseIntensity,
			bloomSaturation,
			baseSaturation);
	}
	bloomCombine->Process(rc.deviceContext);
	UnbindShaderResources(rc.deviceContext);
}

void PostProccess::ToneMapping(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
{
	toneMap->SetOperator(static_cast<DirectX::ToneMapPostProcess::Operator>(toneMapOperatorIndex));
	toneMap->SetTransferFunction(static_cast<DirectX::ToneMapPostProcess::TransferFunction>(transferFunctionIndex));
	toneMap->SetColorRotation(static_cast<DirectX::ToneMapPostProcess::ColorPrimaryRotation>(colorRotationIndex));
	toneMap->SetHDRSourceTexture(colorMap);
	toneMap->SetExposure(exposure);
	toneMap->SetST2084Parameter(paperWhiteNits);
	toneMap->Process(rc.deviceContext);
	UnbindShaderResources(rc.deviceContext);
}

void PostProccess::SSAO(const RenderContext& rc, ID3D11ShaderResourceView* depthMap)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	Matrix viewProjection = rc.camera->GetView() * rc.camera->GetProjection();
	Matrix inverseViewProjection = viewProjection.Invert();

	CbSSAO cb = {};
	cb.viewTransform = rc.camera->GetView();
	cb.inverseViewProjectionTransform = inverseViewProjection;
	cb.projectionTransform = rc.camera->GetProjection();
	cb.zBufferParameteres = Vector4::Zero;
	cb.radius = ssaoRadius;
	cb.intensity = ssaoIntensity;
	cb.minDistance = ssaoMinDistance;
	cb.maxDistance = ssaoMaxDistance;
	dc->UpdateSubresource(SSAOConstantBuffer.Get(), 0, 0, &cb, 0, 0);

	DrawFullscreen(rc, SSAOPixelShader.Get(), depthMap, SSAOConstantBuffer.Get());
}

void PostProccess::ApplySSAO(
	const RenderContext& rc,
	ID3D11ShaderResourceView* colorMap,
	ID3D11ShaderResourceView* ssaoMap)
{
	DrawFullscreen(rc, SSAOCompositePixelShader.Get(), colorMap, nullptr, ssaoMap);
}

void PostProccess::RadialBlur(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	CbRadialBlur cb = {};
	cb.radius = radialBlurRadius;
	cb.samplingCount = max(radialBlurSamplingCount, 1);
	cb.center = radialBlurCenter;
	cb.maskRadius = radialBlurMaskRadius;
	dc->UpdateSubresource(radialBlurConstantBuffer.Get(), 0, 0, &cb, 0, 0);

	DrawFullscreen(rc, radialBlurPixelShader.Get(), colorMap, radialBlurConstantBuffer.Get());
}

void PostProccess::Vignette(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	CbVignette cb = {};
	cb.color = vignetteColor;
	cb.center = vignetteCenter;
	cb.intensity = vignetteIntensity;
	cb.smoothness = vignetteSmoothness;
	cb.rounded = vignetteRounded ? 1.0f : 0.0f;
	cb.roundness = vignetteRoundness;
	dc->UpdateSubresource(vignetteConstantBuffer.Get(), 0, 0, &cb, 0, 0);

	DrawFullscreen(rc, vignettePixelShader.Get(), colorMap, vignetteConstantBuffer.Get());
}

void PostProccess::ChromaticAberration(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	CbChromaticAberration cb = {};
	cb.amount = chromaticAberrationAmount;
	cb.maxSamples = max(chromaticAberrationMaxSamples, 1);
	cb.shift[0] = chromaticAberrationShift[0];
	cb.shift[1] = chromaticAberrationShift[1];
	cb.shift[2] = chromaticAberrationShift[2];
	dc->UpdateSubresource(chromaticAberrationConstantBuffer.Get(), 0, 0, &cb, 0, 0);

	DrawFullscreen(rc, chromaticAberrationPixelShader.Get(), colorMap, chromaticAberrationConstantBuffer.Get());
}

void PostProccess::DrawFullscreen(
	const RenderContext& rc,
	ID3D11PixelShader* pixelShader,
	ID3D11ShaderResourceView* colorMap,
	ID3D11Buffer* constantBuffer,
	ID3D11ShaderResourceView* colorMap2)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	UINT stride = sizeof(FullscreenVertex);
	UINT offset = 0;
	ID3D11Buffer* vertexBuffers[] = {fullscreenVertexBuffer.Get()};
	ID3D11SamplerState* sampler = rc.renderState->GetSamplerState(SamplerState::LinearClamp);
	ID3D11ShaderResourceView* srvs[] = {colorMap, colorMap2};
	const UINT srvCount = colorMap2 ? 2 : 1;

	dc->IASetInputLayout(fullscreenInputLayout.Get());
	dc->IASetVertexBuffers(0, _countof(vertexBuffers), vertexBuffers, &stride, &offset);
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	dc->VSSetShader(fullscreenVertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader, nullptr, 0);
	dc->PSSetShaderResources(0, srvCount, srvs);
	if (constantBuffer)
	{
		dc->PSSetConstantBuffers(2, 1, &constantBuffer);
	}
	dc->PSSetSamplers(0, 1, &sampler);
	dc->Draw(4, 0);

	ID3D11Buffer* nullBuffer = nullptr;
	ID3D11SamplerState* nullSampler = nullptr;
	dc->IASetVertexBuffers(0, 1, &nullBuffer, &stride, &offset);
	dc->VSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);
	dc->PSSetSamplers(0, 1, &nullSampler);
	UnbindShaderResources(dc);
}

void PostProccess::BasicEffect(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
{
	const auto effect = static_cast<DirectX::BasicPostProcess::Effect>(basicEffectIndex);
	basicEffect->SetEffect(effect);
	basicEffect->SetSourceTexture(colorMap);

	if (effect == DirectX::BasicPostProcess::GaussianBlur_5x5)
	{
		basicEffect->SetGaussianParameter(gaussianMultiplier);
	}
	else if (effect == DirectX::BasicPostProcess::BloomExtract)
	{
		basicEffect->SetBloomExtractParameter(finalBloomThreshold);
	}
	else if (effect == DirectX::BasicPostProcess::BloomBlur)
	{
		basicEffect->SetBloomBlurParameters(true, finalBloomBlurSize, finalBloomBlurBrightness);
	}

	basicEffect->Process(rc.deviceContext);
	UnbindShaderResources(rc.deviceContext);
}

void PostProccess::RenderFinal(
	const RenderContext& rc,
	ID3D11ShaderResourceView* colorMap,
	RenderTarget* workBufferA,
	RenderTarget* workBufferB,
	RenderTarget* outputBuffer)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	RenderTarget* workBuffers[] = {workBufferA, workBufferB};
	ID3D11ShaderResourceView* source = colorMap;
	int workBufferIndex = 0;
	int passCount = 1;
	if (enableRadialBlur) ++passCount;
	if (enableVignette) ++passCount;
	if (enableChromaticAberration) ++passCount;
	if (enableBasicEffect) ++passCount;

	auto renderPass = [&](auto draw)
		{
			--passCount;
			RenderTarget* target = passCount == 0
				? outputBuffer
				: workBuffers[workBufferIndex];

			target->Clear(dc);
			target->Activate(dc);
			draw(source);
			if (target != outputBuffer)
			{
				target->Deactivate(dc);
				source = target->GetSRV();
				workBufferIndex = 1 - workBufferIndex;
			}
		};

	renderPass([&](ID3D11ShaderResourceView* passSource)
		{
			if (enableToneMapping)
			{
				ToneMapping(rc, passSource);
			}
			else
			{
				Copy(rc, passSource);
			}
		});

	if (enableRadialBlur)
	{
		renderPass([&](ID3D11ShaderResourceView* passSource)
			{
				RadialBlur(rc, passSource);
			});
	}

	if (enableChromaticAberration)
	{
		renderPass([&](ID3D11ShaderResourceView* passSource)
			{
				ChromaticAberration(rc, passSource);
			});
	}

	if (enableVignette)
	{
		renderPass([&](ID3D11ShaderResourceView* passSource)
			{
				Vignette(rc, passSource);
			});
	}

	if (enableBasicEffect)
	{
		renderPass([&](ID3D11ShaderResourceView* passSource)
			{
				BasicEffect(rc, passSource);
			});
	}
}

void PostProccess::DrawGUI()
{
	// Extract bright pixels for Bloom.
	if (ImGui::TreeNode("Bloom Extract"))
	{
		ImGui::Checkbox("Enable##BloomExtract", &enableBloomExtract);
		ImGui::DragFloat("Threshold", &bloomThreshold, 0.01f, 0.0f, 10.0f);
		ImGui::TreePop();
	}

	// Blur the Bloom texture.
	if (ImGui::TreeNode("Bloom Blur"))
	{
		ImGui::Checkbox("Enable##BloomBlur", &enableBloomBlur);
		ImGui::DragFloat("Size", &bloomBlurSize, 0.1f, 0.0f, 64.0f);
		ImGui::DragFloat("Brightness", &bloomBlurBrightness, 0.01f, 0.0f, 10.0f);
		ImGui::TreePop();
	}

	// Combine the base scene and Bloom texture.
	if (ImGui::TreeNode("Dual Effect"))
	{
		ImGui::Checkbox("Enable##DualEffect", &enableDualEffect);
		if (ImGui::BeginCombo("Effect", DualEffectName(dualEffectIndex)))
		{
			for (int i = 0; i < DirectX::DualPostProcess::Effect_Max; ++i)
			{
				const bool selected = dualEffectIndex == i;
				if (ImGui::Selectable(DualEffectName(i), selected))
				{
					dualEffectIndex = i;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::DragFloat("Bloom Intensity", &bloomIntensity, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Base Intensity", &baseIntensity, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Bloom Saturation", &bloomSaturation, 0.01f, 0.0f, 3.0f);
		ImGui::DragFloat("Base Saturation", &baseSaturation, 0.01f, 0.0f, 3.0f);
		ImGui::DragFloat("Merge Weight 1", &mergeWeight1, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Merge Weight 2", &mergeWeight2, 0.01f, 0.0f, 10.0f);
		ImGui::TreePop();
	}

	// Convert HDR color for display.
	if (ImGui::TreeNode("Tone Mapping"))
	{
		ImGui::Checkbox("Enable##ToneMapping", &enableToneMapping);
		if (ImGui::BeginCombo("Operator", ToneMapOperatorName(toneMapOperatorIndex)))
		{
			for (int i = 0; i < DirectX::ToneMapPostProcess::Operator_Max; ++i)
			{
				const bool selected = toneMapOperatorIndex == i;
				if (ImGui::Selectable(ToneMapOperatorName(i), selected))
				{
					toneMapOperatorIndex = i;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::BeginCombo("Transfer Function", TransferFunctionName(transferFunctionIndex)))
		{
			for (int i = 0; i < DirectX::ToneMapPostProcess::TransferFunction_Max; ++i)
			{
				const bool selected = transferFunctionIndex == i;
				if (ImGui::Selectable(TransferFunctionName(i), selected))
				{
					transferFunctionIndex = i;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::BeginCombo("Color Rotation", ColorRotationName(colorRotationIndex)))
		{
			for (int i = 0; i <= DirectX::ToneMapPostProcess::HDTV_to_DCI_P3_D65; ++i)
			{
				const bool selected = colorRotationIndex == i;
				if (ImGui::Selectable(ColorRotationName(i), selected))
				{
					colorRotationIndex = i;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::DragFloat("Exposure", &exposure, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("Paper White Nits", &paperWhiteNits, 1.0f, 1.0f, 10000.0f);
		ImGui::TreePop();
	}

	// Add depth-based contact shadows in screen space.
	if (ImGui::TreeNode("SSAO"))
	{
		ImGui::Checkbox("Enable##SSAO", &enableSSAO);
		ImGui::DragFloat("Radius", &ssaoRadius, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Intensity", &ssaoIntensity, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("Min Distance", &ssaoMinDistance, 0.0001f, 0.0001f, 0.2f);
		ImGui::DragFloat("Max Distance", &ssaoMaxDistance, 0.001f, 0.01f, 3.0f);
		ImGui::TreePop();
	}

	// Blur outward from the screen center.
	if (ImGui::TreeNode("Radial Blur"))
	{
		ImGui::Checkbox("Enable##RadialBlur", &enableRadialBlur);
		ImGui::DragFloat("Radius", &radialBlurRadius, 1.0f, 0.0f, 512.0f);
		ImGui::DragInt("Sampling Count", &radialBlurSamplingCount, 1, 1, 64);
		ImGui::DragFloat2("Center", &radialBlurCenter.x, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Mask Radius", &radialBlurMaskRadius, 1.0f, 0.0f, 512.0f);
		ImGui::TreePop();
	}

	// Vignette effect to darken the edges of the screen.
	if (ImGui::TreeNode("Vignette"))
	{
		ImGui::Checkbox("Enable##Vignette", &enableVignette);
		ImGui::ColorEdit3("Color", &vignetteColor.x);
		ImGui::DragFloat2("Center", &vignetteCenter.x, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Intensity", &vignetteIntensity, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("Smoothness", &vignetteSmoothness, 0.01f, 0.01f, 10.0f);
		ImGui::Checkbox("Rounded", &vignetteRounded);
		if (vignetteRounded)
		{
			ImGui::DragFloat("Roundness", &vignetteRoundness, 0.01f, 0.1f, 8.0f);
		}
		ImGui::TreePop();
	}

	// Chromatic aberration effect to simulate lens distortion.
	if (ImGui::TreeNode("Chromatic Aberration"))
	{
		ImGui::Checkbox("Enable##ChromaticAberration", &enableChromaticAberration);
		ImGui::DragFloat("Amount", &chromaticAberrationAmount, 0.01f, 0.0f, 1.0f);
		ImGui::DragInt("Max Samples", &chromaticAberrationMaxSamples, 1, 1, 64);
		ImGui::ColorEdit3("Shift0", &chromaticAberrationShift[0].x);
		ImGui::ColorEdit3("Shift1", &chromaticAberrationShift[1].x);
		ImGui::ColorEdit3("Shift2", &chromaticAberrationShift[2].x);
		ImGui::TreePop();
	}

	// Apply one final BasicPostProcess effect.
	if (ImGui::TreeNode("Final Basic Effect"))
	{
		ImGui::Checkbox("Enable##FinalBasicEffect", &enableBasicEffect);
		if (ImGui::BeginCombo("Effect", BasicEffectName(basicEffectIndex)))
		{
			for (int i = 0; i < DirectX::BasicPostProcess::Effect_Max; ++i)
			{
				const bool selected = basicEffectIndex == i;
				if (ImGui::Selectable(BasicEffectName(i), selected))
				{
					basicEffectIndex = i;
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::DragFloat("Gaussian Multiplier", &gaussianMultiplier, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Bloom Threshold", &finalBloomThreshold, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Bloom Blur Size", &finalBloomBlurSize, 0.1f, 0.0f, 64.0f);
		ImGui::DragFloat("Bloom Blur Brightness", &finalBloomBlurBrightness, 0.01f, 0.0f, 10.0f);
		ImGui::TreePop();
	}
}
void PostProccess::UnbindShaderResources(ID3D11DeviceContext* dc)
{
	ID3D11ShaderResourceView* srvs[] = {nullptr, nullptr};
	dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

const char* PostProccess::BasicEffectName(int index)
{
	switch (index)
	{
	case DirectX::BasicPostProcess::Copy: return "Copy";
	case DirectX::BasicPostProcess::Monochrome: return "Monochrome";
	case DirectX::BasicPostProcess::Sepia: return "Sepia";
	case DirectX::BasicPostProcess::DownScale_2x2: return "DownScale 2x2";
	case DirectX::BasicPostProcess::DownScale_4x4: return "DownScale 4x4";
	case DirectX::BasicPostProcess::GaussianBlur_5x5: return "GaussianBlur 5x5";
	case DirectX::BasicPostProcess::BloomExtract: return "BloomExtract";
	case DirectX::BasicPostProcess::BloomBlur: return "BloomBlur";
	default: return "Unknown";
	}
}

const char* PostProccess::DualEffectName(int index)
{
	switch (index)
	{
	case DirectX::DualPostProcess::Merge: return "Merge";
	case DirectX::DualPostProcess::BloomCombine: return "BloomCombine";
	default: return "Unknown";
	}
}

const char* PostProccess::ToneMapOperatorName(int index)
{
	switch (index)
	{
	case DirectX::ToneMapPostProcess::None: return "None";
	case DirectX::ToneMapPostProcess::Saturate: return "Saturate";
	case DirectX::ToneMapPostProcess::Reinhard: return "Reinhard";
	case DirectX::ToneMapPostProcess::ACESFilmic: return "ACESFilmic";
	default: return "Unknown";
	}
}

const char* PostProccess::TransferFunctionName(int index)
{
	switch (index)
	{
	case DirectX::ToneMapPostProcess::Linear: return "Linear";
	case DirectX::ToneMapPostProcess::SRGB: return "SRGB";
	case DirectX::ToneMapPostProcess::ST2084: return "ST2084";
	default: return "Unknown";
	}
}

const char* PostProccess::ColorRotationName(int index)
{
	switch (index)
	{
	case DirectX::ToneMapPostProcess::HDTV_to_UHDTV: return "HDTV to UHDTV";
	case DirectX::ToneMapPostProcess::DCI_P3_D65_to_UHDTV: return "DCI-P3 D65 to UHDTV";
	case DirectX::ToneMapPostProcess::HDTV_to_DCI_P3_D65: return "HDTV to DCI-P3 D65";
	default: return "Unknown";
	}
}

