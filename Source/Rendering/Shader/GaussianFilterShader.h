// GaussianFilterShader.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include "Rendering/Shader/Shader.h"
#include "Rendering/Core/Graphics.h"

class GaussianFilterShader : public SpriteShader
{
public:
	GaussianFilterShader(ID3D11Device* device);
	~GaussianFilterShader() {}

	void Begin(const RenderContext& rc) override;
	void Update(
		const RenderContext& rc,
		ID3D11ShaderResourceView* srv,
		Vector2 textureSize,
		const Color& color) override;
	void End(const RenderContext& rc) override;

	static constexpr int KernelMax = 25;
	int kernelSize = 5;
	float sigma = 20.0f;

private:
	//	シェーダー用
	struct CbGaussianFilter
	{
		Vector4				weights[KernelMax * KernelMax];
		float				kernelSize;
		Vector2				texcel;
		float				dummy;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
};
