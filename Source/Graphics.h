// Graphics.h
#pragma once
#include <string>
#include <vector>

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <windows.h>
#include "RenderState.h"
#include "RenderTarget.h"
#include "PrimitiveRenderer.h"
#include "ShapeRenderer.h"
#include "ModelRenderer.h"
#include "SpriteRenderer.h"
#include "ShadowMapRenderer.h"
#include "SkyBoxRenderer.h"
#include "TrailRenderer.h"

namespace Game
{
	enum class FrameBufferId
	{
		Display,
		Scene,
		Luminance,
		BloomWork,
		SSAO,
		PostProcess,
		PostProcess2,

		EnumCount
	};

	class Graphics
	{
	private:
		Graphics() = default;
		~Graphics() = default;

	public:
		static Game::Graphics& Instance()
		{
			static Graphics instance;
			return instance;
		}

		void Initialize(HWND hWnd);
		void Present(UINT syncInterval);
		void RequestToggleBorderlessFullscreen();
		bool IsBorderlessFullscreen() const { return borderlessFullscreen; }

		HWND GetWindowHandle() { return hWnd; }
		ID3D11Device* GetDevice() { return device.Get(); }
		ID3D11DeviceContext* GetDeviceContext() { return immediateContext.Get(); }
		RenderState* GetRenderState() { return renderState.get(); }
		RenderTarget* GetFrameBuffer(FrameBufferId frameBufferId) { return frameBuffers[static_cast<int>(frameBufferId)].get(); }
		PrimitiveRenderer* GetPrimitiveRenderer() const { return primitiveRenderer.get(); }
		TrailRenderer* GetTrailRenderer() const { return trailRenderer.get(); }
		ShapeRenderer* GetShapeRenderer() const { return shapeRenderer.get(); }
		ModelRenderer* GetModelRenderer() const { return modelRenderer.get(); }
		SpriteRenderer* GetSpriteRenderer() const { return spriteRenderer.get(); }
		ShadowMapRenderer* GetShadowMapRenderer() const { return shadowMapRenderer.get(); }
		SkyBoxRenderer* GetSkyBoxRenderer()    const { return skyBoxRenderer.get(); }

		// IBLテクスチャ SRV取得
		ID3D11ShaderResourceView* GetIBLDiffuseIEM()     const { return iblDiffuseIEM.Get(); }
		ID3D11ShaderResourceView* GetIBLSpecularPMREM()  const { return iblSpecularPMREM.Get(); }
		ID3D11ShaderResourceView* GetIBLGGXLUT()         const { return iblGGXLUT.Get(); }
		const std::string& GetSkyMapName() const { return skyMapName; }
		bool LoadSkyMap(const std::string& name);
		void RefreshSkyMapList();
		void DrawSkyMapGUI();

		static float ScreenWidth;
		static float ScreenHeight;

	private:
		void Resize(UINT width, UINT height);
		void RecreateFrameBuffers(UINT screenWidth, UINT screenHeight);
		void ToggleBorderlessFullscreen();

	private:
		HWND hWnd = nullptr;
		Microsoft::WRL::ComPtr<ID3D11Device>		device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext>	immediateContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain>		swapchain;

		std::unique_ptr<RenderTarget> frameBuffers[static_cast<int>(FrameBufferId::EnumCount)];
		std::unique_ptr<RenderState>		renderState;
		std::unique_ptr<PrimitiveRenderer>	primitiveRenderer;
		std::unique_ptr<TrailRenderer>	trailRenderer;
		std::unique_ptr<ShapeRenderer>		shapeRenderer;
		std::unique_ptr<ModelRenderer>		modelRenderer;
		std::unique_ptr<SpriteRenderer>		spriteRenderer;
		std::unique_ptr<ShadowMapRenderer>	shadowMapRenderer;
		std::unique_ptr<SkyBoxRenderer>		skyBoxRenderer;

		// IBLテクスチャ (Data/lut_ggx.dds, Data/specular_pmrem.dds, Data/diffuse_iem.dds)
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblGGXLUT;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblSpecularPMREM;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblDiffuseIEM;
		std::string skyMapName = "Default";
		std::vector<std::string> skyMapNames;

		bool requestToggleBorderlessFullscreen = false;
		bool borderlessFullscreen = false;
		LONG_PTR windowedStyle = 0;
		LONG_PTR windowedExStyle = 0;
		WINDOWPLACEMENT windowedPlacement = { sizeof(WINDOWPLACEMENT) };
	};
}
