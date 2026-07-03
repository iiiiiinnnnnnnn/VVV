// Graphics.cpp
#include "DebugUtil.h"
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

		frameBuffers[static_cast<int>(FrameBufferId::BloomWork)] =
			std::make_unique<RenderTarget>(
			device.Get(),
			bloomWidth,
			bloomHeight,
			DXGI_FORMAT_R16G16B16A16_FLOAT);

		frameBuffers[static_cast<int>(FrameBufferId::SSAO)] =
			std::make_unique<RenderTarget>(
			device.Get(),
			screenWidth,
			screenHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM);

		// 最終PostProcessはフル解像度
		frameBuffers[static_cast<int>(FrameBufferId::PostProcess)] =
			std::make_unique<RenderTarget>(
			device.Get(),
			screenWidth,
			screenHeight,
			DXGI_FORMAT_R16G16B16A16_FLOAT);

		frameBuffers[static_cast<int>(FrameBufferId::PostProcess2)] =
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
		RefreshSkyMapList();
		LoadSkyMap("Default");
	}

	void Graphics::Present(UINT syncInterval)
	{
		swapchain->Present(syncInterval, 0);
	}

	bool Graphics::LoadSkyMap(const std::string& name)
	{
		if (name.empty())
			return false;

		const std::filesystem::path skyDir = "Data/Sky";
		const std::filesystem::path lutPath = skyDir / (name + "_lut_ggx.dds");
		const std::filesystem::path specularPath = skyDir / (name + "_specular_pmrem.dds");
		const std::filesystem::path diffusePath = skyDir / (name + "_diffuse_iem.dds");

		if (!std::filesystem::exists(lutPath) ||
			!std::filesystem::exists(specularPath) ||
			!std::filesystem::exists(diffusePath))
		{
			return false;
		}

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> newGGXLUT;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> newSpecularPMREM;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> newDiffuseIEM;

		HRESULT hr = GpuResourceUtils::LoadTexture(
			device.Get(),
			lutPath.generic_string().c_str(),
			newGGXLUT.GetAddressOf());
		if (FAILED(hr)) return false;

		hr = GpuResourceUtils::LoadTexture(
			device.Get(),
			specularPath.generic_string().c_str(),
			newSpecularPMREM.GetAddressOf());
		if (FAILED(hr)) return false;

		hr = GpuResourceUtils::LoadTexture(
			device.Get(),
			diffusePath.generic_string().c_str(),
			newDiffuseIEM.GetAddressOf());
		if (FAILED(hr)) return false;

		iblGGXLUT = newGGXLUT;
		iblSpecularPMREM = newSpecularPMREM;
		iblDiffuseIEM = newDiffuseIEM;
		skyMapName = name;
		return true;
	}

	void Graphics::RefreshSkyMapList()
	{
		skyMapNames.clear();

		const std::filesystem::path skyDir = "Data/Sky";
		if (!std::filesystem::exists(skyDir))
		{
			return;
		}

		for (const auto& entry : std::filesystem::directory_iterator(skyDir))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			const std::filesystem::path path = entry.path();
			if (path.extension() != ".dds")
			{
				continue;
			}

			const std::string filename = path.filename().generic_string();
			const std::string suffix = "_specular_pmrem.dds";
			if (filename.size() <= suffix.size() ||
				filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0)
			{
				continue;
			}

			const std::string name = filename.substr(0, filename.size() - suffix.size());
			if (std::filesystem::exists(skyDir / (name + "_lut_ggx.dds")) &&
				std::filesystem::exists(skyDir / (name + "_diffuse_iem.dds")))
			{
				skyMapNames.push_back(name);
			}
		}

		std::sort(skyMapNames.begin(), skyMapNames.end());
	}

	void Graphics::DrawSkyMapGUI()
	{
		if (ImGui::Button("Refresh Sky Maps"))
		{
			RefreshSkyMapList();
		}

		if (skyMapNames.empty())
		{
			ImGui::TextDisabled("No complete sky map set found.");
			return;
		}

		int currentIndex = 0;
		for (int i = 0; i < static_cast<int>(skyMapNames.size()); ++i)
		{
			if (skyMapNames[i] == skyMapName)
			{
				currentIndex = i;
				break;
			}
		}

		const char* preview =
			currentIndex < static_cast<int>(skyMapNames.size())
				? skyMapNames[currentIndex].c_str()
				: skyMapName.c_str();

		if (ImGui::BeginCombo("Sky Map", preview))
		{
			for (int i = 0; i < static_cast<int>(skyMapNames.size()); ++i)
			{
				const bool selected = skyMapNames[i] == skyMapName;
				if (ImGui::Selectable(skyMapNames[i].c_str(), selected))
				{
					LoadSkyMap(skyMapNames[i]);
				}

				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
}
