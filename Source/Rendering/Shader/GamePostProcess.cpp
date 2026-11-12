#include "Resource/ResourceManager.h"
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Drag.h>
#include <ImGuiToolKit/Color.h>
#include <Dx11Kit/Dx11Kit.h>
#include <ProfileKit/ProfileKit.h>
#include "Rendering/Shader/GamePostProcess.h"
#include <cmath>
#include <stdexcept>

#include "Rendering/Core/Graphics.h"
#include "imgui.h"

namespace Game
{
	void PostProcess::CopySettingsFrom(const PostProcess& source)
	{
		enableDepthOfField = source.enableDepthOfField;
		dofFocusDistance = source.dofFocusDistance;
		dofFocusRange = source.dofFocusRange;
		dofBlurRadius = source.dofBlurRadius;
		dofNearStrength = source.dofNearStrength;
		dofFarStrength = source.dofFarStrength;

		// 水中のフォグ・色補正・吸収
		enableUnderwater = source.enableUnderwater;
		underwaterColor = source.underwaterColor;
		underwaterDensity = source.underwaterDensity;
		underwaterTint = source.underwaterTint;
		underwaterAbsorption = source.underwaterAbsorption;

		// ブルーム用の高輝度抽出
		enableBloomExtract = source.enableBloomExtract;
		bloomThreshold = source.bloomThreshold;

		// ブルームのぼかし
		enableBloomBlur = source.enableBloomBlur;
		bloomBlurSize = source.bloomBlurSize;
		bloomBlurBrightness = source.bloomBlurBrightness;

		// ブルーム合成・画像の混合
		enableDualEffect = source.enableDualEffect;
		dualEffectIndex = source.dualEffectIndex;
		bloomIntensity = source.bloomIntensity;
		baseIntensity = source.baseIntensity;
		bloomSaturation = source.bloomSaturation;
		baseSaturation = source.baseSaturation;
		mergeWeight1 = source.mergeWeight1;
		mergeWeight2 = source.mergeWeight2;

		// トーンマッピング・出力色空間
		enableToneMapping = source.enableToneMapping;
		toneMapOperatorIndex = source.toneMapOperatorIndex;
		transferFunctionIndex = source.transferFunctionIndex;
		colorRotationIndex = source.colorRotationIndex;
		exposure = source.exposure;
		paperWhiteNits = source.paperWhiteNits;

		// 色相・彩度・明るさ
		enableColorFilter = source.enableColorFilter;
		colorFilterHueShift = source.colorFilterHueShift;
		colorFilterSaturation = source.colorFilterSaturation;
		colorFilterBrightness = source.colorFilterBrightness;

		// 暗部・中間色・明部のグラデーション
		enableColorGradient = source.enableColorGradient;
		colorGradientStrength = source.colorGradientStrength;
		colorGradientShadows = source.colorGradientShadows;
		colorGradientMidtones = source.colorGradientMidtones;
		colorGradientHighlights = source.colorGradientHighlights;

		// 放射ブラー
		enableRadialBlur = source.enableRadialBlur;
		radialBlurRadius = source.radialBlurRadius;
		radialBlurSamplingCount = source.radialBlurSamplingCount;
		radialBlurCenter = source.radialBlurCenter;
		radialBlurMaskRadius = source.radialBlurMaskRadius;

		// ビネット
		enableVignette = source.enableVignette;
		vignetteColor = source.vignetteColor;
		vignetteCenter = source.vignetteCenter;
		vignetteIntensity = source.vignetteIntensity;
		vignetteSmoothness = source.vignetteSmoothness;
		vignetteRounded = source.vignetteRounded;
		vignetteRoundness = source.vignetteRoundness;

		// 色収差
		enableChromaticAberration = source.enableChromaticAberration;
		chromaticAberrationAmount = source.chromaticAberrationAmount;
		chromaticAberrationMaxSamples = source.chromaticAberrationMaxSamples;

		// スクリーンスペースのアンビエントオクルージョン
		enableSSAO = source.enableSSAO;
		ssaoRadius = source.ssaoRadius;
		ssaoIntensity = source.ssaoIntensity;
		ssaoMinDistance = source.ssaoMinDistance;
		ssaoMaxDistance = source.ssaoMaxDistance;

		// FXAAによるアンチエイリアス
		enableFXAA = source.enableFXAA;

		// 最後に適用する基本エフェクト
		enableBasicEffect = source.enableBasicEffect;
		basicEffectIndex = source.basicEffectIndex;
		gaussianMultiplier = source.gaussianMultiplier;
		finalBloomThreshold = source.finalBloomThreshold;
		finalBloomBlurSize = source.finalBloomBlurSize;
		finalBloomBlurBrightness = source.finalBloomBlurBrightness;

		// 色収差で使用する各チャンネルのシフト色
		for (int i = 0; i < 3; ++i) chromaticAberrationShift[i] = source.chromaticAberrationShift[i];
	}

	void PostProcess::PrepareSceneColor(const RenderContext& rc, RenderTarget* scene,
		RenderTarget* luminance, RenderTarget* bloomWork, RenderTarget* ssao,
		RenderTarget* workA, RenderTarget* workB)
	{
		PROFILE_FUNCTION();
		auto* dc = rc.deviceContext;
		// UI が重なる前の HDR シーンに適用する。深度を使う処理は呼び出し側が深度をクリアする前に終える
		const auto previousOutput = RenderOutput::CaptureOutput(dc);
		ID3D11ShaderResourceView* sceneColorMap = scene->GetSRV();
		if (IsSSAOEnabled())
		{
			// AO の白は遮蔽なしを表す。元のシーン色を残したまま workB に遮蔽を合成する
			ssao->Clear(dc, 1, 1, 1, 1);
			RenderOutput::Bind(dc, ssao, nullptr, false);
			SSAO(rc, rc.depthTarget->GetSRV());

			workB->Clear(dc);
			RenderOutput::Bind(dc, workB, nullptr, false);
			ApplySSAO(rc, scene->GetSRV(), ssao->GetSRV());
			sceneColorMap = workB->GetSRV();
		}

		luminance->Clear(dc);
		RenderOutput::Bind(dc, luminance, nullptr, false);
		if (IsBloomExtractEnabled()) LuminanceExtraction(rc, sceneColorMap);
		else Copy(rc, sceneColorMap);

		// 2 次元のぼかしを横・縦へ分離し、サンプル数を抑える。縦パスの出力は luminance に戻す
		if (IsBloomBlurEnabled())
		{
			bloomWork->Clear(dc);
			RenderOutput::Bind(dc, bloomWork, nullptr, false);
			BloomBlur(rc, luminance->GetSRV(), true);

			luminance->Clear(dc);
			RenderOutput::Bind(dc, luminance, nullptr, false);
			BloomBlur(rc, bloomWork->GetSRV(), false);
		}

		// 以降の入力と、呼び出し側へ渡す完成画像を workA に統一する
		workA->Clear(dc);
		RenderOutput::Bind(dc, workA, nullptr, false);
		if (IsDualEffectEnabled()) Bloom(rc, sceneColorMap, luminance->GetSRV());
		else Copy(rc, sceneColorMap);

		// 同じテクスチャを SRV と RTV に同時指定できないため、workB に描いてから workA へ戻す
		if (enableUnderwater && rc.underwaterBlend > 0.0f && rc.depthTarget->GetSRV())
		{
			RenderOutput::Bind(dc, workB, nullptr, false);
			Underwater(rc, workA->GetSRV(), rc.depthTarget->GetSRV());
			RenderOutput::Bind(dc, workA, nullptr, false);
			Copy(rc, workB->GetSRV());
		}

		if (enableDepthOfField && dofBlurRadius > 0.0f && (dofNearStrength > 0.0f || dofFarStrength > 0.0f))
		{
			DepthOfField(rc, workA, workB);
			RenderOutput::Bind(dc, workA, nullptr, false);
			Copy(rc, workB->GetSRV());
		}

		RenderOutput::RestoreOutput(dc, previousOutput);
	}

	void PostProcess::DepthOfField(const RenderContext& rc, RenderTarget* source, RenderTarget* output)
	{
		PROFILE_FUNCTION();
		auto* device = Game::Graphics::Instance().GetDevice();
		auto* dc = rc.deviceContext;

		// 未使用のシーンでは専用シェーダーと定数バッファを作らない
		if (!dofPixelShader)
		{
			dofBlurPixelShader = std::make_unique<Gpu::PixelShader>(
				device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/DepthOfFieldBlurPS.cso").c_str());
			dofConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, static_cast<UINT>(sizeof(CbDepthOfField)));
			dofPixelShader = std::make_unique<Gpu::PixelShader>(
				device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/DepthOfFieldPS.cso").c_str());
		}

		// 奇数サイズは切り上げ、最小サイズでも 0 にならない半解像度を使う
		const UINT width = std::max(1u, (source->GetWidth() + 1) / 2);
		const UINT height = std::max(1u, (source->GetHeight() + 1) / 2);
		if (!dofWorkA || dofWorkA->GetWidth() != width || dofWorkA->GetHeight() != height)
		{
			dofWorkA = std::make_unique<RenderTarget>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
			dofWorkB = std::make_unique<RenderTarget>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
		}

		// HLSL の cbuffer と同じ順序・16 バイト単位の配置で渡す。逆射影で深度をビュー空間へ戻す
		static_assert(sizeof(CbDepthOfField) == 96);
		CbDepthOfField cb{};
		cb.inverseProjection = rc.camera->GetProjection().Invert();
		cb.focusDistance = dofFocusDistance;
		cb.focusRange = dofFocusRange;
		cb.nearStrength = dofNearStrength;
		cb.farStrength = dofFarStrength;

		RenderOutput::Bind(dc, dofWorkA.get(), nullptr, false);
		Copy(rc, source->GetSRV());

		// シェーダーは中心から片側 4 点を読む。最遠点が指定した「元解像度の半径」になる UV 間隔を渡す
		cb.blurStep = Vector2(dofBlurRadius / (4.0f * source->GetWidth()), 0.0f);
		dofConstantBuffer->Update(dc, &cb);
		RenderOutput::Bind(dc, dofWorkB.get(), nullptr, false);
		DrawFullscreen(rc, *dofBlurPixelShader, dofWorkA->GetSRV(), dofConstantBuffer.get());

		cb.blurStep = Vector2(0.0f, dofBlurRadius / (4.0f * source->GetHeight()));
		dofConstantBuffer->Update(dc, &cb);
		RenderOutput::Bind(dc, dofWorkA.get(), nullptr, false);
		DrawFullscreen(rc, *dofBlurPixelShader, dofWorkB->GetSRV(), dofConstantBuffer.get());

		// ぼかし画像は全体で共通。深度から求めた混合率で元画像のピントを残す簡易 DoF
		RenderOutput::Bind(dc, output, nullptr, false);
		DrawFullscreen(rc, *dofPixelShader, source->GetSRV(), dofConstantBuffer.get(),
			dofWorkA->GetSRV(), rc.depthTarget->GetSRV());
	}

	void PostProcess::Uninit()
	{
		*this = PostProcess{};
	}

	void PostProcess::Init()
	{
		if (initialized) return;
		ID3D11Device* device = Game::Graphics::Instance().GetDevice();
		static_assert(sizeof(CbUnderwater) % 16 == 0);
		underwaterPixelShader = std::make_unique<Gpu::PixelShader>(
			device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/UnderwaterPS.cso").c_str());
		underwaterConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbUnderwater));

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

		// Color Filter
		colorFilterPixelShader = std::make_unique<Gpu::PixelShader>(
			device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/ColorFilterPS.cso").c_str());
		colorFilterConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbColorFilter));

		// SSAO
		SSAOPixelShader = std::make_unique<Gpu::PixelShader>(
			device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/SSAOPS.cso").c_str());
		SSAOCompositePixelShader = std::make_unique<Gpu::PixelShader>(
			device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/SSAOCompositePS.cso").c_str());
		SSAOConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbSSAO));

		// Radial Blur
		radialBlurPixelShader = std::make_unique<Gpu::PixelShader>(
			device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/RadialBlurPS.cso").c_str());
		radialBlurConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbRadialBlur));

		// Vignette
		vignettePixelShader = std::make_unique<Gpu::PixelShader>(
			device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/VignettePS.cso").c_str());
		vignetteConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbVignette));

		// Chromatic Aberration
		chromaticAberrationPixelShader = std::make_unique<Gpu::PixelShader>(
			device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/ChromaticAberrationPS.cso").c_str());
		chromaticAberrationConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbChromaticAberration));

		// FXAA
		FXAAPixelShader = std::make_unique<Gpu::PixelShader>(
			device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/FXAAPS.cso").c_str());

		fullscreenRenderer = std::make_unique<FullScreenRenderer>(device);
		initialized = true;
	}

	void PostProcess::Copy(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
	{
		copyEffect->SetEffect(DirectX::BasicPostProcess::Copy);
		copyEffect->SetSourceTexture(colorMap);
		copyEffect->Process(rc.deviceContext);
		UnbindShaderResources(rc.deviceContext);
	}

	void PostProcess::LuminanceExtraction(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
	{
		bloomExtract->SetEffect(DirectX::BasicPostProcess::BloomExtract);
		bloomExtract->SetSourceTexture(colorMap);
		bloomExtract->SetBloomExtractParameter(bloomThreshold);
		bloomExtract->Process(rc.deviceContext);
		UnbindShaderResources(rc.deviceContext);
	}

	void PostProcess::BloomBlur(const RenderContext& rc, ID3D11ShaderResourceView* bloomMap, bool horizontal)
	{
		bloomBlur->SetEffect(DirectX::BasicPostProcess::BloomBlur);
		bloomBlur->SetSourceTexture(bloomMap);
		bloomBlur->SetBloomBlurParameters(horizontal, bloomBlurSize, bloomBlurBrightness);
		bloomBlur->Process(rc.deviceContext);
		UnbindShaderResources(rc.deviceContext);
	}

	void PostProcess::Bloom(
		const RenderContext& rc,
		ID3D11ShaderResourceView* colorMap,
		ID3D11ShaderResourceView* bloomMap)
	{
		PROFILE_FUNCTION();
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

	void PostProcess::ToneMapping(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
	{
		PROFILE_FUNCTION();
		toneMap->SetOperator(static_cast<DirectX::ToneMapPostProcess::Operator>(toneMapOperatorIndex));
		toneMap->SetTransferFunction(static_cast<DirectX::ToneMapPostProcess::TransferFunction>(transferFunctionIndex));
		toneMap->SetColorRotation(static_cast<DirectX::ToneMapPostProcess::ColorPrimaryRotation>(colorRotationIndex));
		toneMap->SetHDRSourceTexture(colorMap);
		toneMap->SetExposure(exposure);
		toneMap->SetST2084Parameter(paperWhiteNits);
		toneMap->Process(rc.deviceContext);
		UnbindShaderResources(rc.deviceContext);
	}

	void PostProcess::ColorFilter(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
	{
		CbColorFilter cb = {};
		cb.hueShift = enableColorFilter ? colorFilterHueShift : 0.0f;
		cb.saturation = enableColorFilter ? colorFilterSaturation : 1.0f;
		cb.brightness = enableColorFilter ? colorFilterBrightness : 1.0f;
		cb.gradientStrength = enableColorGradient ? colorGradientStrength : 0.0f;
		cb.gradientShadows = colorGradientShadows;
		cb.gradientMidtones = colorGradientMidtones;
		cb.gradientHighlights = colorGradientHighlights;
		colorFilterConstantBuffer->Update(rc.deviceContext, &cb);

		DrawFullscreen(
			rc, *colorFilterPixelShader, colorMap, colorFilterConstantBuffer.get());
	}

	void PostProcess::Underwater(const RenderContext& rc, ID3D11ShaderResourceView* colorMap,
		ID3D11ShaderResourceView* depthMap)
	{
		CbUnderwater cb{};
		// 深度からワールド座標を復元し、水面ローカルへ変換して光が水中を進む長さを求める
		cb.inverseViewProjection = (rc.camera->GetView() * rc.camera->GetProjection()).Invert();
		cb.inverseWaterWorld = rc.underwaterInverseWorld;
		cb.cameraPosition = rc.camera->GetEye();
		cb.blend = rc.underwaterBlend;
		cb.fogColor = underwaterColor;
		cb.absorption = underwaterAbsorption;
		cb.density = underwaterDensity;
		cb.tint = underwaterTint;
		underwaterConstantBuffer->Update(rc.deviceContext, &cb);
		DrawFullscreen(rc, *underwaterPixelShader, colorMap, underwaterConstantBuffer.get(), depthMap);
	}

	void PostProcess::SSAO(const RenderContext& rc, ID3D11ShaderResourceView* depthMap)
	{
		PROFILE_FUNCTION();
		ID3D11DeviceContext* dc = rc.deviceContext;

		// 深度からワールド位置を復元してビュー空間の Z に変換し、中心と周辺の前後差で遮蔽を調べる
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
		cb.distanceFogParams = {
			rc.renderSettings.distanceFogStart,
			rc.renderSettings.distanceFogEnd,
			rc.renderSettings.distanceFogStrength,
			rc.renderSettings.distanceFogEnabled ? 1.0f : 0.0f};
		SSAOConstantBuffer->Update(dc, &cb);

		DrawFullscreen(rc, *SSAOPixelShader, depthMap, SSAOConstantBuffer.get());
	}

	void PostProcess::ApplySSAO(
		const RenderContext& rc,
		ID3D11ShaderResourceView* colorMap,
		ID3D11ShaderResourceView* ssaoMap)
	{
		DrawFullscreen(rc, *SSAOCompositePixelShader, colorMap, nullptr, ssaoMap);
	}

	void PostProcess::RadialBlur(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
	{
		ID3D11DeviceContext* dc = rc.deviceContext;

		// 常設効果が OFF でもヒット演出などの一時効果は使える。強度 1 の基準量は共通の設定値
		const float runtimeRadius = radialBlurRadius * runtimeRadialBlurIntensity;

		CbRadialBlur cb = {};
		cb.radius = enableRadialBlur
			? radialBlurRadius + runtimeRadius
			: runtimeRadius;
		cb.samplingCount = std::max(radialBlurSamplingCount, 1);
		cb.center = radialBlurCenter;
		cb.maskRadius = radialBlurMaskRadius;

		radialBlurConstantBuffer->Update(dc, &cb);

		DrawFullscreen(rc, *radialBlurPixelShader, colorMap, radialBlurConstantBuffer.get());
	}

	void PostProcess::ChromaticAberration(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
	{
		ID3D11DeviceContext* dc = rc.deviceContext;

		const float runtimeAmount =
			chromaticAberrationAmount * runtimeChromaticAberrationIntensity;

		CbChromaticAberration cb = {};
		cb.amount = enableChromaticAberration
			? chromaticAberrationAmount + runtimeAmount
			: runtimeAmount;
		cb.maxSamples = std::max(chromaticAberrationMaxSamples, 1);
		cb.shift[0] = chromaticAberrationShift[0];
		cb.shift[1] = chromaticAberrationShift[1];
		cb.shift[2] = chromaticAberrationShift[2];

		chromaticAberrationConstantBuffer->Update(dc, &cb);

		DrawFullscreen(rc, *chromaticAberrationPixelShader, colorMap, chromaticAberrationConstantBuffer.get());
	}

	void PostProcess::Vignette(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
	{
		ID3D11DeviceContext* dc = rc.deviceContext;

		const float baseVignetteIntensity =
			enableVignette ? vignetteIntensity : 0.0f;
		const float totalIntensity =
			baseVignetteIntensity + runtimeVignetteIntensity;
		// 暗くする強さは加算し、色はその強さの比率で混ぜる。一時効果だけなら一時効果の色になる
		const float runtimeBlend = totalIntensity > 0.001f
			? runtimeVignetteIntensity / totalIntensity
			: 0.0f;

		CbVignette cb = {};
		cb.color = {
			vignetteColor.x + (runtimeVignetteColor.x - vignetteColor.x) * runtimeBlend,
			vignetteColor.y + (runtimeVignetteColor.y - vignetteColor.y) * runtimeBlend,
			vignetteColor.z + (runtimeVignetteColor.z - vignetteColor.z) * runtimeBlend,
			vignetteColor.w + (runtimeVignetteColor.w - vignetteColor.w) * runtimeBlend};
		cb.center = vignetteCenter;
		cb.intensity = totalIntensity;
		cb.smoothness = vignetteSmoothness;
		cb.rounded = vignetteRounded ? 1.0f : 0.0f;
		cb.roundness = vignetteRoundness;
		cb.tintColor = runtimeColorTint;
		cb.tintIntensity = runtimeColorTintIntensity;

		vignetteConstantBuffer->Update(dc, &cb);

		DrawFullscreen(rc, *vignettePixelShader, colorMap, vignetteConstantBuffer.get());
	}

	void PostProcess::DrawFullscreen(
		const RenderContext& rc,
		const Gpu::PixelShader& pixelShader,
		ID3D11ShaderResourceView* colorMap,
		const Gpu::ConstantBuffer* constantBuffer,
		ID3D11ShaderResourceView* colorMap2,
		ID3D11ShaderResourceView* colorMap3)
	{
		ID3D11DeviceContext* dc = rc.deviceContext;
		ID3D11ShaderResourceView* srvs[] = {colorMap, colorMap2, colorMap3};
		const UINT srvCount = colorMap3 ? 3 : colorMap2 ? 2 : 1;

		// 全画面処理は画素を置き換える。直前の透明描画や深度テストの状態を引き継がない
		rc.graphics->OpaqueBlendState->Bind(dc);
		rc.graphics->DepthDisabledState->Bind(dc);
		rc.graphics->SolidRasterizerState->Bind(dc);
		pixelShader.Bind(dc);
		Gpu::PixelShader::SetShaderResources(dc, 0, std::span<ID3D11ShaderResourceView* const>(srvs, srvCount));
		if (constantBuffer)
		{
			constantBuffer->BindPS(dc, 2);
		}
		rc.graphics->LinearClampSamplerState->BindPS(dc, 0);
		fullscreenRenderer->Draw(dc);

		// 次のパスで入力テクスチャを描画先として再利用できるよう、SRV の参照を外す
		Gpu::PixelShader::Unbind(dc);
		Gpu::SamplerState::UnbindPS(dc, 0);
		UnbindShaderResources(dc);
	}

	void PostProcess::BasicEffect(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
	{
		PROFILE_FUNCTION();
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

	void PostProcess::FXAA(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
	{
		PROFILE_FUNCTION();
		DrawFullscreen(rc, *FXAAPixelShader, colorMap, nullptr);
	}

	void PostProcess::RenderFinal(
		const RenderContext& rc,
		ID3D11ShaderResourceView* colorMap,
		RenderTarget* workBufferA,
		RenderTarget* workBufferB,
		RenderTarget* outputBuffer)
	{
		PROFILE_FUNCTION();
		ID3D11DeviceContext* dc = rc.deviceContext;
		RenderTarget* workBuffers[] = {workBufferA, workBufferB};
		ID3D11ShaderResourceView* source = colorMap;
		int workBufferIndex = 0;

		// 常設設定が OFF でも、一時演出や画面の色付けに必要なパスは実行する
		const bool useRadialBlur =
			enableRadialBlur || runtimeRadialBlurIntensity > 0.001f;

		const bool useChromaticAberration =
			enableChromaticAberration || runtimeChromaticAberrationIntensity > 0.001f;

		const bool useVignette =
			enableVignette || runtimeVignetteIntensity > 0.001f ||
			runtimeColorTintIntensity > 0.001f;

		// ToneMapping を切っても Copy を 1 パス実行するため、初期値は 1。最後のパスだけ出力先へ直描きする
		int passCount = 1;
		if (enableColorFilter || enableColorGradient) ++passCount;
		if (useRadialBlur) ++passCount;
		if (useChromaticAberration) ++passCount;
		if (useVignette) ++passCount;
		if (enableBasicEffect) ++passCount;
		if (enableFXAA) ++passCount;

		auto renderPass = [&](auto draw)
		{
			--passCount;

			RenderTarget* target = passCount == 0
				? outputBuffer
				: workBuffers[workBufferIndex];

			target->Clear(dc);
			RenderOutput::Bind(dc, target, nullptr, false);

			draw(source);

			// 中間結果を次の入力にし、書き込み先を交互に切り替えて読み書きの競合を避ける
			if (target != outputBuffer)
			{
				source = target->GetSRV();
				workBufferIndex = 1 - workBufferIndex;
			}
		};

		// 最終パス後は outputBuffer をバインドしたままにし、呼び出し側のデバッグ描画・UI の描画先とする
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

		if (enableColorFilter || enableColorGradient)
		{
			renderPass([&](ID3D11ShaderResourceView* passSource)
			{
				ColorFilter(rc, passSource);
			});
		}

		if (useRadialBlur)
		{
			renderPass([&](ID3D11ShaderResourceView* passSource)
			{
				RadialBlur(rc, passSource);
			});
		}

		if (useChromaticAberration)
		{
			renderPass([&](ID3D11ShaderResourceView* passSource)
			{
				ChromaticAberration(rc, passSource);
			});
		}

		if (useVignette)
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

		if (enableFXAA)
		{
			renderPass([&](ID3D11ShaderResourceView* passSource)
			{
				FXAA(rc, passSource);
			});
		}
	}

	void PostProcess::DrawGUI()
	{
		if (ImGuiToolKit::TreeNode(u8"被写界深度"))
		{
			ImGuiToolKit::Checkbox(u8"有効##DoF", &enableDepthOfField);
			ImGuiToolKit::DragFloat(u8"ピント距離##DoF", &dofFocusDistance, 0.1f, 0.01f, 10000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGuiToolKit::DragFloat(u8"ぼけへの移行距離##DoF", &dofFocusRange, 0.1f, 0.01f, 10000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGuiToolKit::DragFloat(u8"ぼけ半径（画素）##DoF", &dofBlurRadius, 0.1f, 0.0f, 64.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			ImGuiToolKit::SliderFloat(u8"手前のぼけ##DoF", &dofNearStrength, 0.0f, 1.0f);
			ImGuiToolKit::SliderFloat(u8"奥のぼけ##DoF", &dofFarStrength, 0.0f, 1.0f);
			ImGui::TreePop();
		}
		if (ImGuiToolKit::TreeNode(u8"水中"))
		{
			ImGuiToolKit::Checkbox(u8"有効##Underwater", &enableUnderwater);
			ImGuiToolKit::ColorEdit3(u8"フォグ色##Underwater", &underwaterColor.x);
			ImGuiToolKit::DragFloat(u8"濃さ##Underwater", &underwaterDensity, 0.005f, 0.0f, 2.0f);
			ImGuiToolKit::DragFloat3(u8"RGB吸収##Underwater", &underwaterAbsorption.x, 0.005f, 0.0f, 2.0f);
			ImGuiToolKit::SliderFloat(u8"色補正##Underwater", &underwaterTint, 0.0f, 1.0f);
			ImGui::TreePop();
		}

		// Extract bright pixels for Bloom.
		if (ImGuiToolKit::TreeNode(u8"ブルーム抽出"))
		{
			ImGuiToolKit::Checkbox(u8"有効##BloomExtract", &enableBloomExtract);
			ImGuiToolKit::DragFloat(u8"しきい値", &bloomThreshold, 0.01f, 0.0f, 10.0f);
			ImGui::TreePop();
		}

		// Blur the Bloom texture.
		if (ImGuiToolKit::TreeNode(u8"ブルームぼかし"))
		{
			ImGuiToolKit::Checkbox(u8"有効##BloomBlur", &enableBloomBlur);
			ImGuiToolKit::DragSizeFloat(u8"ぼかしサイズ", &bloomBlurSize, 0.1f, 0.0f, 64.0f);
			ImGuiToolKit::DragFloat(u8"明るさ", &bloomBlurBrightness, 0.01f, 0.0f, 10.0f);
			ImGui::TreePop();
		}

		// Combine the base scene and Bloom texture.
		if (ImGuiToolKit::TreeNode(u8"合成エフェクト"))
		{
			ImGuiToolKit::Checkbox(u8"有効##DualEffect", &enableDualEffect);
			if (ImGuiToolKit::BeginCombo(u8"エフェクト", DualEffectName(dualEffectIndex)))
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
			ImGuiToolKit::DragFloat(u8"ブルーム強度", &bloomIntensity, 0.01f, 0.0f, 10.0f);
			ImGuiToolKit::DragFloat(u8"元画像の強度", &baseIntensity, 0.01f, 0.0f, 10.0f);
			ImGuiToolKit::DragFloat(u8"ブルーム彩度", &bloomSaturation, 0.01f, 0.0f, 3.0f);
			ImGuiToolKit::DragFloat(u8"元画像の彩度", &baseSaturation, 0.01f, 0.0f, 3.0f);
			ImGuiToolKit::DragFloat(u8"合成比率1", &mergeWeight1, 0.01f, 0.0f, 10.0f);
			ImGuiToolKit::DragFloat(u8"合成比率2", &mergeWeight2, 0.01f, 0.0f, 10.0f);
			ImGui::TreePop();
		}

		// Convert HDR color for display.
		if (ImGuiToolKit::TreeNode(u8"トーンマッピング"))
		{
			ImGuiToolKit::Checkbox(u8"有効##ToneMapping", &enableToneMapping);
			if (ImGuiToolKit::BeginCombo(u8"方式", ToneMapOperatorName(toneMapOperatorIndex)))
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
			if (ImGuiToolKit::BeginCombo(u8"伝達関数", TransferFunctionName(transferFunctionIndex)))
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
			if (ImGuiToolKit::BeginCombo(u8"色空間変換", ColorRotationName(colorRotationIndex)))
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
			ImGuiToolKit::DragFloat(u8"露出", &exposure, 0.01f, 0.0f, 5.0f);
			ImGuiToolKit::DragFloat(u8"基準白輝度（nit）", &paperWhiteNits, 1.0f, 1.0f, 10000.0f);
			ImGui::TreePop();
		}

		if (ImGuiToolKit::TreeNode(u8"カラーフィルター"))
		{
			ImGuiToolKit::Checkbox(u8"有効##ColorFilter", &enableColorFilter);
			ImGuiToolKit::SliderFloat(u8"色相シフト", &colorFilterHueShift, 0.0f, 360.0f, "%.1f deg");
			ImGuiToolKit::SliderFloat(u8"彩度", &colorFilterSaturation, 0.0f, 2.0f);
			ImGuiToolKit::SliderFloat(u8"明るさ", &colorFilterBrightness, 0.0f, 2.0f);
			if (ImGuiToolKit::Button(u8"初期値に戻す##ColorFilter"))
			{
				colorFilterHueShift = 0.0f;
				colorFilterSaturation = 1.0f;
				colorFilterBrightness = 1.0f;
			}
			ImGui::TreePop();
		}

		if (ImGuiToolKit::TreeNode(u8"カラーグレーディング"))
		{
			ImGuiToolKit::Checkbox(u8"有効##ColorGradient", &enableColorGradient);
			ImGuiToolKit::SliderFloat(u8"適用強度", &colorGradientStrength, 0.0f, 1.0f);
			ImGuiToolKit::ColorEdit3(u8"暗部", &colorGradientShadows.x);
			ImGuiToolKit::ColorEdit3(u8"中間色", &colorGradientMidtones.x);
			ImGuiToolKit::ColorEdit3(u8"明部", &colorGradientHighlights.x);
			if (ImGuiToolKit::Button(u8"初期値に戻す##ColorGradient"))
			{
				const PostProcess defaults;
				colorGradientStrength = defaults.colorGradientStrength;
				colorGradientShadows = defaults.colorGradientShadows;
				colorGradientMidtones = defaults.colorGradientMidtones;
				colorGradientHighlights = defaults.colorGradientHighlights;
			}
			ImGui::TreePop();
		}

		// Add depth-based contact shadows in screen space.
		if (ImGui::TreeNode("SSAO"))
		{
			ImGuiToolKit::Checkbox(u8"有効##SSAO", &enableSSAO);
			ImGuiToolKit::DragSizeFloat(u8"半径##SSAO", &ssaoRadius, 0.01f, 0.0f, 10.0f);
			ImGuiToolKit::DragFloat(u8"強度##SSAO", &ssaoIntensity, 0.01f, 0.0f, 5.0f);
			ImGuiToolKit::DragFloat(u8"最小距離##SSAO", &ssaoMinDistance, 0.0001f, 0.0001f, 0.2f);
			ImGuiToolKit::DragFloat(u8"最大距離##SSAO", &ssaoMaxDistance, 0.001f, 0.01f, 3.0f);
			ImGui::TreePop();
		}

		// Blur outward from the screen center.
		if (ImGuiToolKit::TreeNode(u8"放射ブラー"))
		{
			ImGuiToolKit::Checkbox(u8"有効##RadialBlur", &enableRadialBlur);
			ImGuiToolKit::DragSizeFloat(u8"半径##RadialBlur", &radialBlurRadius, 1.0f, 0.0f, 512.0f);
			ImGuiToolKit::DragInt(u8"サンプル数##RadialBlur", &radialBlurSamplingCount, 1, 1, 64);
			ImGuiToolKit::DragFloat2(u8"中心##RadialBlur", &radialBlurCenter.x, 0.01f, 0.0f, 1.0f);
			ImGuiToolKit::DragSizeFloat(u8"マスク半径##RadialBlur", &radialBlurMaskRadius, 1.0f, 0.0f, 512.0f);
			ImGui::TreePop();
		}

		// Vignette effect to darken the edges of the screen.
		if (ImGuiToolKit::TreeNode(u8"ビネット"))
		{
			ImGuiToolKit::Checkbox(u8"有効##Vignette", &enableVignette);
			ImGuiToolKit::ColorEdit3(u8"色##Vignette", &vignetteColor.x);
			ImGuiToolKit::DragFloat2(u8"中心##Vignette", &vignetteCenter.x, 0.01f, 0.0f, 1.0f);
			ImGuiToolKit::DragFloat(u8"強度##Vignette", &vignetteIntensity, 0.01f, 0.0f, 5.0f);
			ImGuiToolKit::DragFloat(u8"滑らかさ##Vignette", &vignetteSmoothness, 0.01f, 0.01f, 10.0f);
			ImGuiToolKit::Checkbox(u8"円形補正", &vignetteRounded);
			if (vignetteRounded)
			{
				ImGuiToolKit::DragFloat(u8"丸み", &vignetteRoundness, 0.01f, 0.1f, 8.0f);
			}
			ImGui::TreePop();
		}

		// Chromatic aberration effect to simulate lens distortion.
		if (ImGuiToolKit::TreeNode(u8"色収差"))
		{
			ImGuiToolKit::Checkbox(u8"有効##ChromaticAberration", &enableChromaticAberration);
			ImGuiToolKit::DragFloat(u8"ずれ量", &chromaticAberrationAmount, 0.01f, 0.0f, 1.0f);
			ImGuiToolKit::DragInt(u8"最大サンプル数", &chromaticAberrationMaxSamples, 1, 1, 64);
			ImGuiToolKit::ColorEdit3(u8"色ずれ0", &chromaticAberrationShift[0].x);
			ImGuiToolKit::ColorEdit3(u8"色ずれ1", &chromaticAberrationShift[1].x);
			ImGuiToolKit::ColorEdit3(u8"色ずれ2", &chromaticAberrationShift[2].x);
			ImGui::TreePop();
		}

		// Apply one final BasicPostProcess effect.
		if (ImGuiToolKit::TreeNode(u8"最終基本エフェクト"))
		{
			ImGuiToolKit::Checkbox(u8"有効##FinalBasicEffect", &enableBasicEffect);
			if (ImGuiToolKit::BeginCombo(u8"エフェクト##FinalBasicEffect", BasicEffectName(basicEffectIndex)))
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
			ImGuiToolKit::DragFloat(u8"ガウシアン倍率", &gaussianMultiplier, 0.01f, 0.0f, 10.0f);
			ImGuiToolKit::DragFloat(u8"ブルームしきい値", &finalBloomThreshold, 0.01f, 0.0f, 10.0f);
			ImGuiToolKit::DragSizeFloat(u8"ブルームぼかしサイズ", &finalBloomBlurSize, 0.1f, 0.0f, 64.0f);
			ImGuiToolKit::DragFloat(u8"ブルームぼかし明るさ", &finalBloomBlurBrightness, 0.01f, 0.0f, 10.0f);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("FXAA"))
		{
			ImGuiToolKit::Checkbox(u8"有効##FXAA", &enableFXAA);
			ImGui::TreePop();
		}
	}
	void PostProcess::UnbindShaderResources(ID3D11DeviceContext* dc)
	{
		Gpu::PixelShader::UnbindShaderResources(dc, 0, 3);
	}

	const char* PostProcess::BasicEffectName(int index)
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

	const char* PostProcess::DualEffectName(int index)
	{
		switch (index)
		{
			case DirectX::DualPostProcess::Merge: return "Merge";
			case DirectX::DualPostProcess::BloomCombine: return "BloomCombine";
			default: return "Unknown";
		}
	}

	const char* PostProcess::ToneMapOperatorName(int index)
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

	const char* PostProcess::TransferFunctionName(int index)
	{
		switch (index)
		{
			case DirectX::ToneMapPostProcess::Linear: return "Linear";
			case DirectX::ToneMapPostProcess::SRGB: return "SRGB";
			case DirectX::ToneMapPostProcess::ST2084: return "ST2084";
			default: return "Unknown";
		}
	}

	const char* PostProcess::ColorRotationName(int index)
	{
		switch (index)
		{
			case DirectX::ToneMapPostProcess::HDTV_to_UHDTV: return "HDTV to UHDTV";
			case DirectX::ToneMapPostProcess::DCI_P3_D65_to_UHDTV: return "DCI-P3 D65 to UHDTV";
			case DirectX::ToneMapPostProcess::HDTV_to_DCI_P3_D65: return "HDTV to DCI-P3 D65";
			default: return "Unknown";
		}
	}

	// Runtime effects management

	void PostProcess::ClearRuntimeEffects()
	{
		runtimeRadialBlurIntensity = 0.0f;
		runtimeChromaticAberrationIntensity = 0.0f;
		runtimeVignetteIntensity = 0.0f;
		runtimeVignetteColor = {0.0f, 0.0f, 0.0f, 1.0f};
		runtimeColorTintIntensity = 0.0f;
		runtimeColorTint = {1.0f, 1.0f, 1.0f, 1.0f};
	}

	void PostProcess::AddRuntimeRadialBlur(float intensity)
	{
		// 同じフレームの複数要求は加算せず、最も強い要求を採用して過剰なぼけを抑える
		runtimeRadialBlurIntensity = std::max(runtimeRadialBlurIntensity, intensity);
	}

	void PostProcess::AddRuntimeChromaticAberration(float intensity)
	{
		runtimeChromaticAberrationIntensity = std::max(runtimeChromaticAberrationIntensity, intensity);
	}

	void PostProcess::AddRuntimeVignette(float intensity, const Color& color)
	{
		if (intensity <= 0.0f) return;
		const float combinedIntensity = std::max(runtimeVignetteIntensity, intensity);
		// 強度を掛けた各色の寄与を加算し、描画時の強度で正規化する
		runtimeVignetteColor = {
			(runtimeVignetteColor.x * runtimeVignetteIntensity + color.x * intensity) / combinedIntensity,
			(runtimeVignetteColor.y * runtimeVignetteIntensity + color.y * intensity) / combinedIntensity,
			(runtimeVignetteColor.z * runtimeVignetteIntensity + color.z * intensity) / combinedIntensity,
			1.0f};
		runtimeVignetteIntensity = combinedIntensity;
	}

	void PostProcess::AddRuntimeColorTint(float intensity, const Color& color)
	{
		// 色は混ぜず、最も強い要求の色で統一する。強度が同じ場合は先に届いた要求を優先する
		if (intensity <= runtimeColorTintIntensity) return;
		runtimeColorTintIntensity = intensity;
		runtimeColorTint = color;
	}

	json PostProcess::CaptureMovieSettings() const
	{
		json settings;
		settings["enableDepthOfField"] = enableDepthOfField;
		settings["dofFocusDistance"] = dofFocusDistance;
		settings["dofFocusRange"] = dofFocusRange;
		settings["dofBlurRadius"] = dofBlurRadius;
		settings["dofNearStrength"] = dofNearStrength;
		settings["dofFarStrength"] = dofFarStrength;

		settings["enableUnderwater"] = enableUnderwater;
		settings["underwaterColor"] = json::array({underwaterColor.x, underwaterColor.y, underwaterColor.z, underwaterColor.w});
		settings["underwaterDensity"] = underwaterDensity;
		settings["underwaterTint"] = underwaterTint;
		settings["underwaterAbsorption"] = json::array({underwaterAbsorption.x, underwaterAbsorption.y, underwaterAbsorption.z});

		settings["enableBloomExtract"] = enableBloomExtract;
		settings["bloomThreshold"] = bloomThreshold;

		settings["enableBloomBlur"] = enableBloomBlur;
		settings["bloomBlurSize"] = bloomBlurSize;
		settings["bloomBlurBrightness"] = bloomBlurBrightness;

		settings["enableDualEffect"] = enableDualEffect;
		settings["dualEffectIndex"] = dualEffectIndex;
		settings["bloomIntensity"] = bloomIntensity;
		settings["baseIntensity"] = baseIntensity;
		settings["bloomSaturation"] = bloomSaturation;
		settings["baseSaturation"] = baseSaturation;
		settings["mergeWeight1"] = mergeWeight1;
		settings["mergeWeight2"] = mergeWeight2;

		settings["enableToneMapping"] = enableToneMapping;
		settings["toneMapOperatorIndex"] = toneMapOperatorIndex;
		settings["transferFunctionIndex"] = transferFunctionIndex;
		settings["colorRotationIndex"] = colorRotationIndex;
		settings["exposure"] = exposure;
		settings["paperWhiteNits"] = paperWhiteNits;

		settings["enableColorFilter"] = enableColorFilter;
		settings["colorFilterHueShift"] = colorFilterHueShift;
		settings["colorFilterSaturation"] = colorFilterSaturation;
		settings["colorFilterBrightness"] = colorFilterBrightness;

		settings["enableColorGradient"] = enableColorGradient;
		settings["colorGradientStrength"] = colorGradientStrength;
		settings["colorGradientShadows"] = json::array({
			colorGradientShadows.x, colorGradientShadows.y, colorGradientShadows.z, colorGradientShadows.w});
		settings["colorGradientMidtones"] = json::array({
			colorGradientMidtones.x, colorGradientMidtones.y, colorGradientMidtones.z, colorGradientMidtones.w});
		settings["colorGradientHighlights"] = json::array({
			colorGradientHighlights.x, colorGradientHighlights.y, colorGradientHighlights.z, colorGradientHighlights.w});

		settings["enableRadialBlur"] = enableRadialBlur;
		settings["radialBlurRadius"] = radialBlurRadius;
		settings["radialBlurSamplingCount"] = radialBlurSamplingCount;
		settings["radialBlurCenter"] = json::array({radialBlurCenter.x, radialBlurCenter.y});
		settings["radialBlurMaskRadius"] = radialBlurMaskRadius;

		settings["enableVignette"] = enableVignette;
		settings["vignetteColor"] = json::array({vignetteColor.x, vignetteColor.y, vignetteColor.z, vignetteColor.w});
		settings["vignetteCenter"] = json::array({vignetteCenter.x, vignetteCenter.y});
		settings["vignetteIntensity"] = vignetteIntensity;
		settings["vignetteSmoothness"] = vignetteSmoothness;
		settings["vignetteRounded"] = vignetteRounded;
		settings["vignetteRoundness"] = vignetteRoundness;

		settings["enableChromaticAberration"] = enableChromaticAberration;
		settings["chromaticAberrationAmount"] = chromaticAberrationAmount;
		settings["chromaticAberrationMaxSamples"] = chromaticAberrationMaxSamples;

		settings["enableSSAO"] = enableSSAO;
		settings["ssaoRadius"] = ssaoRadius;
		settings["ssaoIntensity"] = ssaoIntensity;
		settings["ssaoMinDistance"] = ssaoMinDistance;
		settings["ssaoMaxDistance"] = ssaoMaxDistance;

		settings["enableFXAA"] = enableFXAA;

		settings["enableBasicEffect"] = enableBasicEffect;
		settings["basicEffectIndex"] = basicEffectIndex;
		settings["gaussianMultiplier"] = gaussianMultiplier;
		settings["finalBloomThreshold"] = finalBloomThreshold;
		settings["finalBloomBlurSize"] = finalBloomBlurSize;
		settings["finalBloomBlurBrightness"] = finalBloomBlurBrightness;

		settings["chromaticAberrationShift"] = json::array();
		for (const Color& value : chromaticAberrationShift)
			settings["chromaticAberrationShift"].push_back(json::array({value.x, value.y, value.z, value.w}));
		return settings;
	}

	void PostProcess::ValidateMovieSettings(const json& inputSettings)
	{
		json settings = inputSettings;
		const json schema = PostProcess{}.CaptureMovieSettings();

		// DoF 追加前のムービーだけを補完する。他の必須項目の欠落や未知の項目は従来どおり拒否する
		if (settings.is_object())
			for (const char* name : {"enableDepthOfField", "dofFocusDistance", "dofFocusRange",
				"dofBlurRadius", "dofNearStrength", "dofFarStrength"})
				if (!settings.contains(name)) settings[name] = schema.at(name);

		if (!settings.is_object() || settings.size() != schema.size())
			throw std::runtime_error("Invalid movie post-process settings.");

		// 初期設定を型と配列長の見本にする。色・ベクトルの各成分も再帰的に検証する
		const auto validate = [](const auto& self, const json& value, const json& expected) -> void
		{
			if (expected.is_array())
			{
				if (!value.is_array() || value.size() != expected.size()) throw std::runtime_error("Invalid movie effect vector.");
				for (size_t i = 0; i < expected.size(); ++i) self(self, value[i], expected[i]);
			}
			else if (expected.is_boolean())
			{
				if (!value.is_boolean()) throw std::runtime_error("Invalid movie effect switch.");
			}
			else
			{
				if (!value.is_number() || !std::isfinite(value.get<double>()) ||
					std::abs(value.get<double>()) > 100000.0 ||
					(expected.is_number_integer() && !value.is_number_integer()))
					throw std::runtime_error("Invalid movie effect value.");
			}
		};
		for (const auto& [name, expected] : schema.items()) validate(validate, settings.at(name), expected);
		const auto index = [&](const char* name, int maximum)
		{
			const int value = settings.at(name).get<int>();
			if (value < 0 || value >= maximum) throw std::runtime_error("Invalid movie effect mode.");
		};
		if (settings.at("dofFocusDistance").get<float>() <= 0.0f || settings.at("dofFocusRange").get<float>() <= 0.0f ||
			settings.at("dofBlurRadius").get<float>() < 0.0f || settings.at("dofBlurRadius").get<float>() > 64.0f ||
			settings.at("dofNearStrength").get<float>() < 0.0f || settings.at("dofNearStrength").get<float>() > 1.0f ||
			settings.at("dofFarStrength").get<float>() < 0.0f || settings.at("dofFarStrength").get<float>() > 1.0f)
			throw std::runtime_error("Invalid movie depth-of-field settings.");
		index("dualEffectIndex", DirectX::DualPostProcess::Effect_Max);
		index("basicEffectIndex", DirectX::BasicPostProcess::Effect_Max);
		index("toneMapOperatorIndex", DirectX::ToneMapPostProcess::Operator_Max);
		index("transferFunctionIndex", DirectX::ToneMapPostProcess::TransferFunction_Max);
		index("colorRotationIndex", DirectX::ToneMapPostProcess::HDTV_to_DCI_P3_D65 + 1);
		if (settings.at("radialBlurSamplingCount").get<int>() < 1 || settings.at("radialBlurSamplingCount").get<int>() > 64 ||
			settings.at("chromaticAberrationMaxSamples").get<int>() < 1 || settings.at("chromaticAberrationMaxSamples").get<int>() > 64)
			throw std::runtime_error("Movie effect sample counts must be between 1 and 64.");
	}

	void PostProcess::ApplyMovieSettings(const json& settings)
	{
		enableDepthOfField = settings.value("enableDepthOfField", false);
		dofFocusDistance = settings.value("dofFocusDistance", 25.0f);
		dofFocusRange = settings.value("dofFocusRange", 20.0f);
		dofBlurRadius = settings.value("dofBlurRadius", 12.0f);
		dofNearStrength = settings.value("dofNearStrength", 1.0f);
		dofFarStrength = settings.value("dofFarStrength", 1.0f);

		enableUnderwater = settings.at("enableUnderwater").get<bool>();
		underwaterColor = Color(
			settings.at("underwaterColor").at(0).get<float>(),
			settings.at("underwaterColor").at(1).get<float>(),
			settings.at("underwaterColor").at(2).get<float>(),
			settings.at("underwaterColor").at(3).get<float>());
		underwaterDensity = settings.at("underwaterDensity").get<float>();
		underwaterTint = settings.at("underwaterTint").get<float>();
		underwaterAbsorption = Vector3(
			settings.at("underwaterAbsorption").at(0).get<float>(),
			settings.at("underwaterAbsorption").at(1).get<float>(),
			settings.at("underwaterAbsorption").at(2).get<float>());

		enableBloomExtract = settings.at("enableBloomExtract").get<bool>();
		bloomThreshold = settings.at("bloomThreshold").get<float>();

		enableBloomBlur = settings.at("enableBloomBlur").get<bool>();
		bloomBlurSize = settings.at("bloomBlurSize").get<float>();
		bloomBlurBrightness = settings.at("bloomBlurBrightness").get<float>();

		enableDualEffect = settings.at("enableDualEffect").get<bool>();
		dualEffectIndex = settings.at("dualEffectIndex").get<int>();
		bloomIntensity = settings.at("bloomIntensity").get<float>();
		baseIntensity = settings.at("baseIntensity").get<float>();
		bloomSaturation = settings.at("bloomSaturation").get<float>();
		baseSaturation = settings.at("baseSaturation").get<float>();
		mergeWeight1 = settings.at("mergeWeight1").get<float>();
		mergeWeight2 = settings.at("mergeWeight2").get<float>();

		enableToneMapping = settings.at("enableToneMapping").get<bool>();
		toneMapOperatorIndex = settings.at("toneMapOperatorIndex").get<int>();
		transferFunctionIndex = settings.at("transferFunctionIndex").get<int>();
		colorRotationIndex = settings.at("colorRotationIndex").get<int>();
		exposure = settings.at("exposure").get<float>();
		paperWhiteNits = settings.at("paperWhiteNits").get<float>();

		enableColorFilter = settings.at("enableColorFilter").get<bool>();
		colorFilterHueShift = settings.at("colorFilterHueShift").get<float>();
		colorFilterSaturation = settings.at("colorFilterSaturation").get<float>();
		colorFilterBrightness = settings.at("colorFilterBrightness").get<float>();

		enableColorGradient = settings.at("enableColorGradient").get<bool>();
		colorGradientStrength = settings.at("colorGradientStrength").get<float>();
		colorGradientShadows = Color(
			settings.at("colorGradientShadows").at(0).get<float>(),
			settings.at("colorGradientShadows").at(1).get<float>(),
			settings.at("colorGradientShadows").at(2).get<float>(),
			settings.at("colorGradientShadows").at(3).get<float>());
		colorGradientMidtones = Color(
			settings.at("colorGradientMidtones").at(0).get<float>(),
			settings.at("colorGradientMidtones").at(1).get<float>(),
			settings.at("colorGradientMidtones").at(2).get<float>(),
			settings.at("colorGradientMidtones").at(3).get<float>());
		colorGradientHighlights = Color(
			settings.at("colorGradientHighlights").at(0).get<float>(),
			settings.at("colorGradientHighlights").at(1).get<float>(),
			settings.at("colorGradientHighlights").at(2).get<float>(),
			settings.at("colorGradientHighlights").at(3).get<float>());

		enableRadialBlur = settings.at("enableRadialBlur").get<bool>();
		radialBlurRadius = settings.at("radialBlurRadius").get<float>();
		radialBlurSamplingCount = settings.at("radialBlurSamplingCount").get<int>();
		radialBlurCenter = Vector2(settings.at("radialBlurCenter").at(0).get<float>(), settings.at("radialBlurCenter").at(1).get<float>());
		radialBlurMaskRadius = settings.at("radialBlurMaskRadius").get<float>();

		enableVignette = settings.at("enableVignette").get<bool>();
		vignetteColor = Color(
			settings.at("vignetteColor").at(0).get<float>(),
			settings.at("vignetteColor").at(1).get<float>(),
			settings.at("vignetteColor").at(2).get<float>(),
			settings.at("vignetteColor").at(3).get<float>());
		vignetteCenter = Vector2(settings.at("vignetteCenter").at(0).get<float>(), settings.at("vignetteCenter").at(1).get<float>());
		vignetteIntensity = settings.at("vignetteIntensity").get<float>();
		vignetteSmoothness = settings.at("vignetteSmoothness").get<float>();
		vignetteRounded = settings.at("vignetteRounded").get<bool>();
		vignetteRoundness = settings.at("vignetteRoundness").get<float>();

		enableChromaticAberration = settings.at("enableChromaticAberration").get<bool>();
		chromaticAberrationAmount = settings.at("chromaticAberrationAmount").get<float>();
		chromaticAberrationMaxSamples = settings.at("chromaticAberrationMaxSamples").get<int>();

		enableSSAO = settings.at("enableSSAO").get<bool>();
		ssaoRadius = settings.at("ssaoRadius").get<float>();
		ssaoIntensity = settings.at("ssaoIntensity").get<float>();
		ssaoMinDistance = settings.at("ssaoMinDistance").get<float>();
		ssaoMaxDistance = settings.at("ssaoMaxDistance").get<float>();

		enableFXAA = settings.at("enableFXAA").get<bool>();

		enableBasicEffect = settings.at("enableBasicEffect").get<bool>();
		basicEffectIndex = settings.at("basicEffectIndex").get<int>();
		gaussianMultiplier = settings.at("gaussianMultiplier").get<float>();
		finalBloomThreshold = settings.at("finalBloomThreshold").get<float>();
		finalBloomBlurSize = settings.at("finalBloomBlurSize").get<float>();
		finalBloomBlurBrightness = settings.at("finalBloomBlurBrightness").get<float>();
		for (size_t i = 0; i < 3; ++i)
		{
			const auto& value = settings.at("chromaticAberrationShift").at(i);
			chromaticAberrationShift[i] = Color(value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>(), value.at(3).get<float>());
		}
	}

}
