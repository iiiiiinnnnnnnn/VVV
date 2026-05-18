#pragma once

#include "Shader.h"
#include "Graphics.h"

class GaussianFilterShader : public SpriteShader
{
public:
	GaussianFilterShader(ID3D11Device* device);
	~GaussianFilterShader() {}

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, ID3D11ShaderResourceView* srv, float r, float g, float b, float a, float elapsedTime) override;
	void ApplyParams(std::shared_ptr<void> params) override;
	void End(const RenderContext& rc) override;

	//	ガウスフィルター
	static constexpr int KernelMax = 25;

	//	シェーダー側への転送用定数バッファ
	struct CbGaussianFilter
	{
		Vector4				weights[KernelMax * KernelMax];
		float				kernelSize;
		Vector2				texcel;
		float				dummy;
	};

	//	ガウスフィルター処理用情報
	struct GaussianFilterData
	{
		int					kernel_size{ 20 };
		float				sigma{ 20.0f };
		Vector2				texture_size{ Graphics::ScreenWidth, Graphics::ScreenHeight };
	};

	GaussianFilterData gaussianFilterData;
	Microsoft::WRL::ComPtr<ID3D11Buffer> gaussianFilterConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussianFilterPixelShader;
	void CalculateGaussianFilterConstant(CbGaussianFilter& constant, const GaussianFilterData& data);
};
