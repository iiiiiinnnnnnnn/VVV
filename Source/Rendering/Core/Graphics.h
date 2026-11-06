#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <atomic>
#include <string>
#include <vector>

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <windows.h>
#include "Rendering/Core/BlendState.h"
#include "Rendering/Core/DepthStencilState.h"
#include "Rendering/Core/RasterizerState.h"
#include "Rendering/Core/SamplerState.h"
#include "Rendering/Core/RenderTarget.h"
#include "Rendering/Renderer/PrimitiveRenderer.h"
#include "Rendering/Renderer/DebugSolidRenderer.h"
#include "Rendering/Renderer/ShapeRenderer.h"
#include "Rendering/Renderer/ModelRenderer.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include "Rendering/Renderer/ShadowMapRenderer.h"
#include "Rendering/Renderer/SkyBoxRenderer.h"
#include "Rendering/Renderer/TrailRenderer.h"

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
		void SetBorderlessFullscreen(bool enabled);
		bool IsBorderlessFullscreen() const { return borderlessFullscreen; }

		HWND GetWindowHandle() { return hWnd; }
		ID3D11Device* GetDevice() { return device.Get(); }
		ID3D11DeviceContext* GetDeviceContext() { return immediateContext.Get(); }
		RenderTarget* GetFrameBuffer(FrameBufferId frameBufferId) { return frameBuffers[static_cast<int>(frameBufferId)].get(); }
		PrimitiveRenderer* GetPrimitiveRenderer() const { return primitiveRenderer.get(); }
		DebugSolidRenderer* GetDebugSolidRenderer() const { return debugSolidRenderer.get(); }
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
		void Resize(UINT width, UINT height);
		void SetWindowMovementLocked(bool locked) { windowMovementLocked = locked; }
		bool IsWindowMovementLocked() const { return windowMovementLocked; }
		Vector2 GetMouseNDC(float mouseX, float mouseY) const;

		static float ScreenWidth;
		static float ScreenHeight;

	private:
		void RecreateFrameBuffers(UINT screenWidth, UINT screenHeight);
		void ToggleBorderlessFullscreen();

	private:
		HWND hWnd = nullptr;
		Microsoft::WRL::ComPtr<ID3D11Device>		device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext>	immediateContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain>		swapchain;

		std::unique_ptr<RenderTarget>		frameBuffers[static_cast<int>(FrameBufferId::EnumCount)];

	public:
		std::unique_ptr<Gpu::BlendState> OpaqueBlendState;
		std::unique_ptr<Gpu::BlendState> TransparentBlendState;
		std::unique_ptr<Gpu::BlendState> AdditiveBlendState;
		std::unique_ptr<Gpu::BlendState> SubtractiveBlendState;
		std::unique_ptr<Gpu::BlendState> MultiplicativeBlendState;
		std::unique_ptr<Gpu::DepthStencilState> DepthWriteState;
		std::unique_ptr<Gpu::DepthStencilState> DepthReadState;
		std::unique_ptr<Gpu::DepthStencilState> DepthWriteOnlyState;
		std::unique_ptr<Gpu::DepthStencilState> DepthDisabledState;
		std::unique_ptr<Gpu::RasterizerState> SolidRasterizerState;
		std::unique_ptr<Gpu::RasterizerState> BackCullRasterizerState;
		std::unique_ptr<Gpu::RasterizerState> WireframeRasterizerState;
		std::unique_ptr<Gpu::RasterizerState> WireframeBackCullRasterizerState;
		std::unique_ptr<Gpu::SamplerState> PointWrapSamplerState;
		std::unique_ptr<Gpu::SamplerState> PointWrapBaseLevelSamplerState;
		std::unique_ptr<Gpu::SamplerState> PointClampSamplerState;
		std::unique_ptr<Gpu::SamplerState> LinearWrapSamplerState;
		std::unique_ptr<Gpu::SamplerState> LinearClampSamplerState;
		std::unique_ptr<Gpu::SamplerState> AnisotropicWrapSamplerState;

	private:
		std::unique_ptr<PrimitiveRenderer>	primitiveRenderer;
		std::unique_ptr<DebugSolidRenderer>	debugSolidRenderer;
		std::unique_ptr<TrailRenderer>		trailRenderer;
		std::unique_ptr<ShapeRenderer>		shapeRenderer;
		std::unique_ptr<ModelRenderer>		modelRenderer;
		std::unique_ptr<SpriteRenderer>		spriteRenderer;
		std::unique_ptr<ShadowMapRenderer>	shadowMapRenderer;
		std::unique_ptr<SkyBoxRenderer>		skyBoxRenderer;

		// IBLテクスチャ (Resources/lut_ggx.dds, Resources/specular_pmrem.dds, Resources/diffuse_iem.dds)
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblGGXLUT;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblSpecularPMREM;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> iblDiffuseIEM;
		std::string skyMapName = "Default";
		std::array<std::filesystem::file_time_type, 3> skyMapUpdated{};
		std::array<std::uintmax_t, 3> skyMapSizes{};
		std::vector<std::string> skyMapNames;

		bool requestToggleBorderlessFullscreen = false;
		std::atomic<int> requestedBorderlessFullscreen{-1};
		bool borderlessFullscreen = false;
		bool windowMovementLocked = false;
		LONG_PTR windowedStyle = 0;
		LONG_PTR windowedExStyle = 0;
		WINDOWPLACEMENT windowedPlacement = { sizeof(WINDOWPLACEMENT) };
	};
}
