// GaussianFilterShader.cpp

#include "GaussianFilterShader.h"
#include "GpuResourceUtils.h"

GaussianFilterShader::GaussianFilterShader(ID3D11Device* device)
{
	// 頂点シェーダー
	GpuResourceUtils::LoadVertexShader(
		device,
		"Data/Shader/SpriteVS.cso",
		SpriteShader::InputElementDescs.data(),
		static_cast<UINT>(SpriteShader::InputElementDescs.size()),
		inputLayout.GetAddressOf(),
		vertexShader.GetAddressOf());

	// ピクセルシェーダー
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/GaussianFilteringPS.cso",
		pixelShader.GetAddressOf());

	// ガウスフィルター用定数バッファ
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbGaussianFilter),
		constantBuffer.GetAddressOf());
}

void GaussianFilterShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
}

void GaussianFilterShader::Update(const RenderContext& rc, ID3D11ShaderResourceView* srv, Vector2 textureSize, Color color, float elapsedTime)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	//	ガウスフィルター用の定数バッファを算出
	CbGaussianFilter constant;
	{
		//	偶数の場合は奇数に直す
		int kernel_size = gaussianFilterData.kernel_size;
		if (kernel_size % 2 == 0)
			kernel_size++;

		constant.kernelSize = static_cast<float>(kernel_size);
		constant.texcel.x = 1.0f / textureSize.x;
		constant.texcel.y = 1.0f / textureSize.y;
		//	重みを算出
		float sum = 0.0f;
		int id = 0;
		for (int y = -kernel_size / 2; y <= kernel_size / 2; y++)
		{
			for (int x = -kernel_size / 2; x <= kernel_size / 2; x++)
			{
				constant.weights[id].x = (float)x;
				constant.weights[id].y = (float)y;
				constant.weights[id].z = (float)exp(-(x * x + y * y) / (2.0f * gaussianFilterData.sigma * gaussianFilterData.sigma)) / (2.0f * DirectX::XM_PI * gaussianFilterData.sigma);
				sum += constant.weights[id].z;
				id++;
			}
		}
		//	平均化
		for (int i = 0; i < kernel_size * kernel_size; i++)
		{
			constant.weights[i].z /= sum;
		}
	}

	//	定数バッファを設定
	dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &constant, 0, 0);

	// 定数バッファ設定
	ID3D11Buffer* cbs[] =
	{
		constantBuffer.Get()
	};
	dc->PSSetConstantBuffers(2, _countof(cbs), cbs);

	dc->PSSetShaderResources(0, 1, &srv);
}

void GaussianFilterShader::ApplyParams(ShaderParamPtr params)
{
	gaussianFilterData = *static_cast<const GaussianFilterData*>(params);
}

void GaussianFilterShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	dc->VSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);

	ID3D11ShaderResourceView* nullSrv = nullptr;
	dc->PSSetShaderResources(0, 1, &nullSrv);
}
