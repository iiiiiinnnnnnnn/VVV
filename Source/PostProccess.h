// PostProccess.h

#pragma once

#include <memory>
#include <d3d11.h>
#include <wrl.h>

#include "PostProcess.h"
#include "RenderContext.h"

class RenderTarget;

class PostProccess
{
public:
	PostProccess();

	void Copy(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
	void LuminanceExtraction(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
	void BloomBlur(const RenderContext& rc, ID3D11ShaderResourceView* bloomMap, bool horizontal);
	void Bloom(const RenderContext& rc, ID3D11ShaderResourceView* colorMap, ID3D11ShaderResourceView* bloomMap);
	void ToneMapping(const RenderContext& rc, ID3D11ShaderResourceView* colorMap);
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
	bool IsRadialBlurEnabled() const { return enableRadialBlur; }
	bool IsVignetteEnabled() const { return enableVignette; }
	bool IsChromaticAberrationEnabled() const { return enableChromaticAberration; }
	bool IsBasicEffectEnabled() const { return enableBasicEffect; }
	bool IsFXAAEnabled() const { return enableFXAA; }
	void DrawGUI();

private:
	void DrawFullscreen(
		const RenderContext& rc,
		ID3D11PixelShader* pixelShader,
		ID3D11ShaderResourceView* colorMap,
		ID3D11Buffer* constantBuffer,
		ID3D11ShaderResourceView* colorMap2 = nullptr);
	void UnbindShaderResources(ID3D11DeviceContext* dc);
	static const char* BasicEffectName(int index);
	static const char* DualEffectName(int index);
	static const char* ToneMapOperatorName(int index);
	static const char* TransferFunctionName(int index);
	static const char* ColorRotationName(int index);

	// Fullscreen pass
	struct FullscreenVertex
	{
		Vector3 position;
		Vector2 texcoord;
	};
	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> fullscreenVertexShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> fullscreenInputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> fullscreenVertexBuffer;

	// DirectXTK post process objects
	std::unique_ptr<DirectX::BasicPostProcess> copyEffect;
	std::unique_ptr<DirectX::BasicPostProcess> bloomExtract;
	std::unique_ptr<DirectX::BasicPostProcess> bloomBlur;
	std::unique_ptr<DirectX::BasicPostProcess> basicEffect;
	std::unique_ptr<DirectX::DualPostProcess> bloomCombine;
	std::unique_ptr<DirectX::ToneMapPostProcess> toneMap;

	// Bloom Extract
	bool enableBloomExtract = true;
	float bloomThreshold = 0.11f;

	// Bloom Blur
	bool enableBloomBlur = true;
	float bloomBlurSize = 4.0f;
	float bloomBlurBrightness = 1.0f;

	// Dual Effect
	bool enableDualEffect = true;
	int dualEffectIndex = DirectX::DualPostProcess::BloomCombine;
	float bloomIntensity = 0.3f;
	float baseIntensity = 1.0f;
	float bloomSaturation = 1.0f;
	float baseSaturation = 1.0f;
	float mergeWeight1 = 1.0f;
	float mergeWeight2 = 1.0f;

	// Tone Mapping
	bool enableToneMapping = true;
	int toneMapOperatorIndex = DirectX::ToneMapPostProcess::ACESFilmic;
	int transferFunctionIndex = DirectX::ToneMapPostProcess::SRGB;
	int colorRotationIndex = DirectX::ToneMapPostProcess::HDTV_to_UHDTV;
	float exposure = 1.0f;
	float paperWhiteNits = 200.0f;

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
	Microsoft::WRL::ComPtr<ID3D11PixelShader> radialBlurPixelShader;
	Microsoft::WRL::ComPtr<ID3D11Buffer> radialBlurConstantBuffer;

	// Vignette
	bool enableVignette = true;
	Color vignetteColor = { 0.2f, 0.2f, 0.2f, 1.0f };
	Vector2 vignetteCenter = { 0.5f, 0.5f };
	float vignetteIntensity = 1.0f;
	float vignetteSmoothness = 2.0f;
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
	};
	Microsoft::WRL::ComPtr<ID3D11PixelShader> vignettePixelShader;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vignetteConstantBuffer;
	
	// Chromatic Aberration
	bool enableChromaticAberration = false;
	float chromaticAberrationAmount = 0.015f;
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
	Microsoft::WRL::ComPtr<ID3D11PixelShader> chromaticAberrationPixelShader;
	Microsoft::WRL::ComPtr<ID3D11Buffer> chromaticAberrationConstantBuffer;

	// SSAO
	bool enableSSAO = true;
	float ssaoRadius = 1.5f;
	float ssaoIntensity = 3.0f;
	float ssaoMinDistance = 0.05f;
	float ssaoMaxDistance = 5.0f;
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
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> SSAOConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> SSAOPixelShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> SSAOCompositePixelShader;

	// FXAA
	bool enableFXAA = true;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> FXAAPixelShader;

	// Final Basic Effect
	bool enableBasicEffect = false;
	int basicEffectIndex = DirectX::BasicPostProcess::Copy;
	float gaussianMultiplier = 1.0f;
	float finalBloomThreshold = 0.25f;
	float finalBloomBlurSize = 4.0f;
	float finalBloomBlurBrightness = 1.0f;
};
