#pragma once
#include "Rendering/Renderer/FullScreenRenderer.h"
#include <Dx11Kit/Dx11Kit.h>

#include <memory>
#include <d3d11.h>
#include <wrl.h>

#include <PostProcess.h>
#include "Rendering/Core/RenderContext.h"
#include "Core/Foundation/Json.h"

class RenderTarget;

namespace Game
{
	class PostProcess
	{
	public:
		PostProcess() = default;
		void Init();
		void Uninit();

		void CopySettingsFrom(const PostProcess& source);
		json CaptureMovieSettings() const;
		static void ValidateMovieSettings(const json& settings);
		void ApplyMovieSettings(const json& settings);
		void PrepareSceneColor(const RenderContext& rc, RenderTarget* scene, RenderTarget* luminance,
			RenderTarget* bloomWork, RenderTarget* ssao, RenderTarget* workA, RenderTarget* workB);
		void Copy(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
		void LuminanceExtraction(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
		void BloomBlur(const RenderContext& rc, ID3D11ShaderResourceView* bloomMap, bool horizontal);
		void Bloom(const RenderContext& rc, ID3D11ShaderResourceView* colorMap, ID3D11ShaderResourceView* bloomMap);
		void ToneMapping(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
		void ColorFilter(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
		void SSAO(const RenderContext& rc, ID3D11ShaderResourceView* depthMap);
		void ApplySSAO(const RenderContext& rc, ID3D11ShaderResourceView* colorMap, ID3D11ShaderResourceView* ssaoMap);
		void RadialBlur(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
		void Vignette(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
		void ChromaticAberration(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
		void BasicEffect(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
		void FXAA(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
		void RenderFinal(
			const RenderContext& rc,
			ID3D11ShaderResourceView* colorMap,
			RenderTarget* workBufferA,
			RenderTarget* workBufferB,
			RenderTarget* outputBuffer);
		bool IsBloomExtractEnabled() const { return enableBloomExtract; }
		bool IsBloomBlurEnabled() const { return enableBloomBlur; }
		bool IsDualEffectEnabled() const { return enableDualEffect; }
		bool IsSSAOEnabled() const { return enableSSAO; }
		bool IsToneMappingEnabled() const { return enableToneMapping; }
		bool IsColorFilterEnabled() const { return enableColorFilter; }
		bool IsColorGradientEnabled() const { return enableColorGradient; }
		bool IsRadialBlurEnabled() const { return enableRadialBlur; }
		bool IsVignetteEnabled() const { return enableVignette; }
		bool IsChromaticAberrationEnabled() const { return enableChromaticAberration; }
		bool IsBasicEffectEnabled() const { return enableBasicEffect; }
		bool IsFXAAEnabled() const { return enableFXAA; }
		void DrawGUI();

		void ClearRuntimeEffects();

		void AddRuntimeRadialBlur(float intensity);
		void AddRuntimeChromaticAberration(float intensity);
		void AddRuntimeVignette(float intensity, const Color& color);
		void AddRuntimeColorTint(float intensity, const Color& color);
		const Color& GetUnderwaterColor() const { return underwaterColor; }

	private:
		// 下水
		void Underwater(const RenderContext& rc, ID3D11ShaderResourceView* colorMap,
			ID3D11ShaderResourceView* depthMap);
		bool enableUnderwater = true;
		Color underwaterColor = {0.023529f, 0.278431f, 0.219608f, 1.0f};
		float underwaterDensity = 0.12f;
		float underwaterTint = 0.12f;
		Vector3 underwaterAbsorption = {0.023f, 0.117f, 0.278f};
		struct CbUnderwater
		{
			Matrix inverseViewProjection;
			Matrix inverseWaterWorld;
			Vector3 cameraPosition;
			float blend;
			Color fogColor;
			Vector3 absorption;
			float density;
			float tint;
			float padding[3];
		};
		std::unique_ptr<Gpu::PixelShader> underwaterPixelShader;
		std::unique_ptr<Gpu::ConstantBuffer> underwaterConstantBuffer;

		void DepthOfField(const RenderContext& rc, RenderTarget* source, RenderTarget* output);
		bool enableDepthOfField = false;
		float dofFocusDistance = 25.0f;
		// ピントから最大ぼけへ移る距離幅。鮮明さを固定する帯域ではない
		float dofFocusRange = 20.0f;
		// 半解像度の作業バッファではなく、元画像の画素数を単位とする
		float dofBlurRadius = 12.0f;
		float dofNearStrength = 1.0f;
		float dofFarStrength = 1.0f;

		struct CbDepthOfField
		{
			Matrix inverseProjection;
			float focusDistance;
			float focusRange;
			float nearStrength;
			float farStrength;
			Vector2 blurStep;
			Vector2 padding;
		};
		std::unique_ptr<RenderTarget> dofWorkA, dofWorkB;
		std::unique_ptr<Gpu::PixelShader> dofBlurPixelShader, dofPixelShader;
		std::unique_ptr<Gpu::ConstantBuffer> dofConstantBuffer;

		void DrawFullscreen(
			const RenderContext& rc,
			const Gpu::PixelShader& pixelShader,
			ID3D11ShaderResourceView* colorMap,
			const Gpu::ConstantBuffer* constantBuffer,
			ID3D11ShaderResourceView* colorMap2 = nullptr,
			ID3D11ShaderResourceView* colorMap3 = nullptr);
		void UnbindShaderResources(ID3D11DeviceContext* dc);
		static const char* BasicEffectName(int index);
		static const char* DualEffectName(int index);
		static const char* ToneMapOperatorName(int index);
		static const char* TransferFunctionName(int index);
		static const char* ColorRotationName(int index);

		// Fullscreen pass
		std::unique_ptr<FullScreenRenderer> fullscreenRenderer;

		// DirectXTK post process objects
		std::unique_ptr<DirectX::BasicPostProcess> copyEffect;
		std::unique_ptr<DirectX::BasicPostProcess> bloomExtract;
		std::unique_ptr<DirectX::BasicPostProcess> bloomBlur;
		std::unique_ptr<DirectX::BasicPostProcess> basicEffect;
		std::unique_ptr<DirectX::DualPostProcess> bloomCombine;
		std::unique_ptr<DirectX::ToneMapPostProcess> toneMap;

		// Bloom Extract
		bool enableBloomExtract = true;
		float bloomThreshold = 0.18f;

		// Bloom Blur
		bool enableBloomBlur = true;
		float bloomBlurSize = 2.0f;
		float bloomBlurBrightness = 0.6f;

		// Dual Effect
		bool enableDualEffect = true;
		int dualEffectIndex = DirectX::DualPostProcess::BloomCombine;
		float bloomIntensity = 0.57f;
		float baseIntensity = 0.5f;
		float bloomSaturation = 1.0f;
		float baseSaturation = 1.0f;
		float mergeWeight1 = 1.0f;
		float mergeWeight2 = 1.0f;

		// Tone Mapping
		bool enableToneMapping = true;
		int toneMapOperatorIndex = DirectX::ToneMapPostProcess::ACESFilmic;
		int transferFunctionIndex = DirectX::ToneMapPostProcess::Linear;
		int colorRotationIndex = DirectX::ToneMapPostProcess::HDTV_to_UHDTV;
		float exposure = 1.45f;
		float paperWhiteNits = 200.0f;

		// Color Filter
		bool enableColorFilter = true;
		float colorFilterHueShift = 0.0f;
		float colorFilterSaturation = 2.0f;
		float colorFilterBrightness = 1.5f;
		bool enableColorGradient = true;
		float colorGradientStrength = 0.25f;
		Color colorGradientShadows = {0.054901961f, 0.054901961f, 0.054901961f, 1.0f};
		Color colorGradientMidtones = {0.078431373f, 0.176470588f, 0.270588235f, 1.0f};
		Color colorGradientHighlights = {1.0f, 1.0f, 1.0f, 1.0f};
		struct CbColorFilter
		{
			float hueShift;
			float saturation;
			float brightness;
			float gradientStrength;
			Color gradientShadows;
			Color gradientMidtones;
			Color gradientHighlights;
		};
		std::unique_ptr<Gpu::PixelShader> colorFilterPixelShader;
		std::unique_ptr<Gpu::ConstantBuffer> colorFilterConstantBuffer;

		// Radial Blur
		bool enableRadialBlur = false;
		float radialBlurRadius = 80.0f;
		int radialBlurSamplingCount = 8;
		Vector2 radialBlurCenter = {0.5f, 0.5f};
		float radialBlurMaskRadius = 150.0f;
		struct CbRadialBlur
		{
			float radius;
			int samplingCount;
			Vector2 center;

			float maskRadius;
			float DUMMY[3];
		};
		std::unique_ptr<Gpu::PixelShader> radialBlurPixelShader;
		std::unique_ptr<Gpu::ConstantBuffer> radialBlurConstantBuffer;

		// Vignette
		bool enableVignette = true;
		Color vignetteColor = {0.0f, 0.0f, 0.0f, 1.0f};
		Vector2 vignetteCenter = {0.5f, 0.5f};
		float vignetteIntensity = 1.29f;
		float vignetteSmoothness = 0.7f;
		bool vignetteRounded = false;
		float vignetteRoundness = 1.0f;
		struct CbVignette
		{
			Color color;
			Vector2 center;
			float intensity;
			float smoothness;

			float rounded;
			float roundness;
			float DUMMY[2];
			Color tintColor;
			float tintIntensity;
			float DUMMY2[3];
		};
		std::unique_ptr<Gpu::PixelShader> vignettePixelShader;
		std::unique_ptr<Gpu::ConstantBuffer> vignetteConstantBuffer;

		// Chromatic Aberration
		bool enableChromaticAberration = false;
		float chromaticAberrationAmount = 0.001f;
		int chromaticAberrationMaxSamples = 10;
		Color chromaticAberrationShift[3] = {
			{1.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f, 0.0f}
		};
		struct CbChromaticAberration
		{
			float amount;
			int maxSamples;
			Vector2 DUMMY;

			Color shift[3];
		};
		std::unique_ptr<Gpu::PixelShader> chromaticAberrationPixelShader;
		std::unique_ptr<Gpu::ConstantBuffer> chromaticAberrationConstantBuffer;

		// SSAO
		bool enableSSAO = true;
		float ssaoRadius = 6.34f;
		float ssaoIntensity = 5.0f;
		float ssaoMinDistance = 0.2f;
		float ssaoMaxDistance = 3.0f;
		struct CbSSAO
		{
			Matrix viewTransform;
			Matrix inverseViewProjectionTransform;
			Matrix projectionTransform;
			Vector4 zBufferParameteres;

			float radius;
			float intensity;
			float minDistance;
			float maxDistance;
			Vector4 distanceFogParams;
		};
		std::unique_ptr<Gpu::ConstantBuffer> SSAOConstantBuffer;
		std::unique_ptr<Gpu::PixelShader> SSAOPixelShader;
		std::unique_ptr<Gpu::PixelShader> SSAOCompositePixelShader;

		// FXAA
		bool enableFXAA = true;
		std::unique_ptr<Gpu::PixelShader> FXAAPixelShader;

		// Final Basic Effect
		bool enableBasicEffect = false;
		int basicEffectIndex = DirectX::BasicPostProcess::Copy;
		float gaussianMultiplier = 1.0f;
		float finalBloomThreshold = 0.25f;
		float finalBloomBlurSize = 4.0f;
		float finalBloomBlurBrightness = 1.0f;

		bool initialized = false;

		// Runtime effects
		float runtimeRadialBlurIntensity = 0.0f;
		float runtimeChromaticAberrationIntensity = 0.0f;
		float runtimeVignetteIntensity = 0.0f;
		Color runtimeVignetteColor = {0.0f, 0.0f, 0.0f, 1.0f};
		float runtimeColorTintIntensity = 0.0f;
		Color runtimeColorTint = {1.0f, 1.0f, 1.0f, 1.0f};
	};
}
