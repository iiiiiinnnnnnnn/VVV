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
	void End(const RenderContext& rc) override;

	//	ガウスフィルター
	static constexpr int KernelMax = 25;

	//	シェーダー側への転送用定数バッファ
	struct gaussian_filter_constants
	{
		DirectX::XMFLOAT4	weights[KernelMax * KernelMax];
		float				kernelSize;
		DirectX::XMFLOAT2	texcel;
		float				dummy;
	};

	//	ガウスフィルター処理用情報
	struct gaussian_filter_datas
	{
		int					kernel_size{ 20 };
		float				sigma{ 20.0f };
		DirectX::XMFLOAT2	texture_size{ Graphics::ScreenWidth, Graphics::ScreenHeight };
	};

	gaussian_filter_datas gaussianFilterData;
	Microsoft::WRL::ComPtr<ID3D11Buffer> gaussianFilterConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussianFilterPixelShader;
	void CalculateGaussianFilterConstant(gaussian_filter_constants& constant, const gaussian_filter_datas& data);
};
