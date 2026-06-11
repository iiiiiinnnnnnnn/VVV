// Graphics.cpp
#include "Misc.h"
#include "Graphics.h"
#include "GpuResourceUtils.h"

namespace Game
{
	float Graphics::ScreenWidth = 0.0f;
	float Graphics::ScreenHeight = 0.0f;

	void Graphics::Initialize(HWND hWnd)
	{
		this->hWnd = hWnd;
		RECT rc;
		GetClientRect(hWnd, &rc);
		UINT screenWidth  = rc.right - rc.left;
		UINT screenHeight = rc.bottom - rc.top;

		ScreenWidth  = static_cast<float>(screenWidth);
		ScreenHeight = static_cast<float>(screenHeight);

		UINT bloomWidth = screenWidth / 4;
		UINT bloomHeight = screenHeight / 4;

		if (bloomWidth < 1)
		{
			bloomWidth = 1;
		}

		if (bloomHeight < 1)
		{
			bloomHeight = 1;
		}

		HRESULT hr = S_OK;

		// デバイス＆スワップチェーンの生成
		{
			UINT createDeviceFlags = 0;
			#if defined(DEBUG) || defined(_DEBUG)
			createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
			#endif

			D3D_FEATURE_LEVEL featureLevels[] =
			{
				D3D_FEATURE_LEVEL_11_0,
				D3D_FEATURE_LEVEL_10_1,
				D3D_FEATURE_LEVEL_10_0,
				D3D_FEATURE_LEVEL_9_3,
				D3D_FEATURE_LEVEL_9_2,
				D3D_FEATURE_LEVEL_9_1,
			};

			DXGI_SWAP_CHAIN_DESC swapchainDesc;
			{
				swapchainDesc.BufferDesc.Width                   = screenWidth;
				swapchainDesc.BufferDesc.Height                  = screenHeight;
				swapchainDesc.BufferDesc.RefreshRate.Numerator   = 60;
				swapchainDesc.BufferDesc.RefreshRate.Denominator = 1;
				swapchainDesc.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
				swapchainDesc.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
				swapchainDesc.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
				swapchainDesc.SampleDesc.Count                   = 1;
				swapchainDesc.SampleDesc.Quality                 = 0;
				swapchainDesc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
				swapchainDesc.BufferCount                        = 2;
				swapchainDesc.OutputWindow                       = hWnd;
				swapchainDesc.Windowed                           = TRUE;
				swapchainDesc.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
				swapchainDesc.Flags                              = 0;
			}

			D3D_FEATURE_LEVEL featureLevel;
			hr = D3D11CreateDeviceAndSwapChain(
				nullptr,
				D3D_DRIVER_TYPE_HARDWARE,
				nullptr,
				createDeviceFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				&swapchainDesc,
				swapchain.GetAddressOf(),
				device.GetAddressOf(),
				&featureLevel,
				immediateContext.GetAddressOf()
			);
			_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		}

		// バックバッファ用 RenderTarget を生成
		frameBuffers[static_cast<int>(FrameBufferId::Display)] =
			std::make_unique<RenderTarget>(
			device.Get(),
			swapchain.Get(),
			screenWidth,
			screenHeight);

		// HDR用シーンバッファはフル解像度
		frameBuffers[static_cast<int>(FrameBufferId::Scene)] =
			std::make_unique<RenderTarget>(
			device.Get(),
			screenWidth,
			screenHeight,
			DXGI_FORMAT_R16G16B16A16_FLOAT);

		// Bloom系は低解像度にする
		// ここが重要。フル解像度のままだと小さい丸が大量に見えやすい。
		frameBuffers[static_cast<int>(FrameBufferId::Luminance)] =
			std::make_unique<RenderTarget>(
			device.Get(),
			bloomWidth,
			bloomHeight,
			DXGI_FORMAT_R16G16B16A16_FLOAT);

		frameBuffers[static_cast<int>(FrameBufferId::BloomTemp)] =
			std::make_unique<RenderTarget>(
			device.Get(),
			bloomWidth,
			bloomHeight,
			DXGI_FORMAT_R16G16B16A16_FLOAT);

		frameBuffers[static_cast<int>(FrameBufferId::BloomBlur)] =
			std::make_unique<RenderTarget>(
			device.Get(),
			bloomWidth,
			bloomHeight,
			DXGI_FORMAT_R16G16B16A16_FLOAT);

		// 最終PostProcessはフル解像度
		frameBuffers[static_cast<int>(FrameBufferId::PostProcess)] =
			std::make_unique<RenderTarget>(
			device.Get(),
			screenWidth,
			screenHeight,
			DXGI_FORMAT_R16G16B16A16_FLOAT);

		// 各レンダラー生成
		renderState       = std::make_unique<RenderState>(device.Get());
		primitiveRenderer = std::make_unique<PrimitiveRenderer>(device.Get());
		trailRenderer	  = std::make_unique<TrailRenderer>(device.Get());
		shapeRenderer     = std::make_unique<ShapeRenderer>(device.Get());
		modelRenderer     = std::make_unique<ModelRenderer>(device.Get());
		spriteRenderer    = std::make_unique<SpriteRenderer>(device.Get());
		shadowMapRenderer = std::make_unique<ShadowMapRenderer>(device.Get());
		skyBoxRenderer    = std::make_unique<SkyBoxRenderer>(device.Get());

		// IBLテクスチャ読み込み
		GpuResourceUtils::LoadTexture(device.Get(), "Data/lut_ggx.dds",        iblGGXLUT.GetAddressOf());
		GpuResourceUtils::LoadTexture(device.Get(), "Data/specular_pmrem.dds", iblSpecularPMREM.GetAddressOf());
		GpuResourceUtils::LoadTexture(device.Get(), "Data/diffuse_iem.dds",    iblDiffuseIEM.GetAddressOf());
	}

	void Graphics::Present(UINT syncInterval)
	{
		swapchain->Present(syncInterval, 0);
	}
}
