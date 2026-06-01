// Graphics.h
#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include "RenderState.h"
#include "PrimitiveRenderer.h"
#include "ShapeRenderer.h"
#include "ModelRenderer.h"
#include "SpriteRenderer.h"
#include "ShadowMapRenderer.h"
#include "SkyBoxRenderer.h"

namespace Game
{
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
		void Clear(float r, float g, float b, float a);
		void SetRenderTargets();
		void Present(UINT syncInterval);

		HWND GetWindowHandle() { return hWnd; }
		ID3D11Device* GetDevice() { return device.Get(); }
		ID3D11DeviceContext* GetDeviceContext() { return immediateContext.Get(); }
		RenderState* GetRenderState() { return renderState.get(); }
		PrimitiveRenderer* GetPrimitiveRenderer() const { return primitiveRenderer.get(); }
		ShapeRenderer* GetShapeRenderer() const { return shapeRenderer.get(); }
		ModelRenderer* GetModelRenderer() const { return modelRenderer.get(); }
		SpriteRenderer* GetSpriteRenderer() const { return spriteRenderer.get(); }
		ShadowMapRenderer* GetShadowMapRenderer() const { return shadowMapRenderer.get(); }
		SkyBoxRenderer* GetSkyBoxRenderer()    const { return skyBoxRenderer.get(); }

		// IBLテクスチャ SRV取得
		ID3D11ShaderResourceView* GetIBLDiffuseIEM()     const { return iblDiffuseIEM.Get(); }
		ID3D11ShaderResourceView* GetIBLSpecularPMREM()  const { return iblSpecularPMREM.Get(); }
		ID3D11ShaderResourceView* GetIBLGGXLUT()         const { return iblGGXLUT.Get(); }

		static float ScreenWidth;
		static float ScreenHeight;

	private:
		HWND hWnd = nullptr;
		Microsoft::WRL::ComPtr<ID3D11Device>			device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext>		immediateContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain>			swapchain;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	renderTargetView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	depthStencilView;
		D3D11_VIEWPORT									viewport;

		std::unique_ptr<RenderState>		renderState;
		std::unique_ptr<PrimitiveRenderer>	primitiveRenderer;
		std::unique_ptr<ShapeRenderer>		shapeRenderer;
		std::unique_ptr<ModelRenderer>		modelRenderer;
		std::unique_ptr<SpriteRenderer>		spriteRenderer;
		std::unique_ptr<ShadowMapRenderer>	shadowMapRenderer;
		std::unique_ptr<SkyBoxRenderer>		skyBoxRenderer;

		// IBLテクスチャ (Data/lut_ggx.dds, Data/specular_pmrem.dds, Data/diffuse_iem.dds)
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblGGXLUT;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblSpecularPMREM;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblDiffuseIEM;
	};
}