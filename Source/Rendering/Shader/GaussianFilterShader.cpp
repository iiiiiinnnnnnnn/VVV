#include "Rendering/Core/PixelShader.h"
#include "Rendering/Core/VertexShader.h"
#include "Rendering/Shader/GaussianFilterShader.h"

GaussianFilterShader::GaussianFilterShader(ID3D11Device* device)
{
	// 頂点シェーダー
	vertexShader = std::make_unique<Gpu::VertexShader>(device, "Resources/Shader/BasicSpriteVS.cso",
		SpriteShader::InputLayout);

	// ピクセルシェーダー
	pixelShader = std::make_unique<Gpu::PixelShader>(device, "Resources/Shader/GaussianFilteringPS.cso");

	// ガウスフィルター用定数バッファ
	constantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbGaussianFilter));
}

void GaussianFilterShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	vertexShader->Bind(dc);
	pixelShader->Bind(dc);
}

void GaussianFilterShader::Update(
	const RenderContext& rc,
	ID3D11ShaderResourceView* srv,
	Vector2 textureSize,
	const Color& color,
	const SpriteRenderParams* params)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	//	ガウスフィルター用の定数バッファを算出
	CbGaussianFilter constant;
	{
		//	偶数の場合は奇数に直す
		if (kernelSize % 2 == 0)
			kernelSize++;

		constant.kernelSize = static_cast<float>(kernelSize);
		constant.texcel.x = 1.0f / textureSize.x;
		constant.texcel.y = 1.0f / textureSize.y;
		//	重みを算出
		float sum = 0.0f;
		int id = 0;
		for (int y = -kernelSize / 2; y <= kernelSize / 2; y++)
		{
			for (int x = -kernelSize / 2; x <= kernelSize / 2; x++)
			{
				constant.weights[id].x = (float)x;
				constant.weights[id].y = (float)y;
				constant.weights[id].z = (float)exp(-(x * x + y * y) / (2.0f * sigma * sigma)) / (2.0f * DirectX::XM_PI * sigma);
				sum += constant.weights[id].z;
				id++;
			}
		}
		//	平均化
		for (int i = 0; i < kernelSize * kernelSize; i++)
		{
			constant.weights[i].z /= sum;
		}
	}

	//	定数バッファを設定
	constantBuffer->Update(dc, &constant);

	// 定数バッファ設定
	constantBuffer->BindPS(dc, 2);

	Gpu::PixelShader::SetShaderResources(dc, 0, srv);
}

void GaussianFilterShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	Gpu::VertexShader::Unbind(dc);
	Gpu::PixelShader::Unbind(dc);
	Gpu::VertexShader::UnbindInputLayout(dc);

	Gpu::PixelShader::UnbindShaderResources(dc, 0);
}
