#pragma once

#include <Dx11Kit/RenderTarget.h>
#include <Dx11Kit/DepthTarget.h>

class RenderOutput
{
  public:
	struct OutputState
	{
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> color;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
		std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
		UINT viewportCount = 0;
	};

	static void Bind(ID3D11DeviceContext* context, const RenderTarget* color, const DepthTarget* depth, bool readOnly);
	static Microsoft::WRL::ComPtr<ID3D11RenderTargetView> GetColorTarget(ID3D11DeviceContext* context);
	static D3D11_VIEWPORT GetViewport(ID3D11DeviceContext* context);
	static void SetViewport(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport);
	static OutputState CaptureOutput(ID3D11DeviceContext* context);
	static void RestoreOutput(ID3D11DeviceContext* context, const OutputState& state);
};

inline void RenderOutput::Bind(ID3D11DeviceContext* context, const RenderTarget* color, const DepthTarget* depth, bool readOnly)
{
	ID3D11RenderTargetView* rtv = color ? color->GetRTV() : nullptr;
	context->OMSetRenderTargets(color ? 1 : 0, color ? &rtv : nullptr, depth ? depth->GetDSV(readOnly) : nullptr);
	if (!color && !depth) return;
	D3D11_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(color ? color->GetWidth() : depth->GetWidth());
	viewport.Height = static_cast<float>(color ? color->GetHeight() : depth->GetHeight());
	viewport.MaxDepth = 1.0f;
	SetViewport(context, viewport);
}

inline Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderOutput::GetColorTarget(ID3D11DeviceContext* context)
{
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
	context->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
	return target;
}

inline D3D11_VIEWPORT RenderOutput::GetViewport(ID3D11DeviceContext* context)
{
	D3D11_VIEWPORT viewport{};
	UINT count = 1;
	context->RSGetViewports(&count, &viewport);
	return viewport;
}

inline void RenderOutput::SetViewport(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport)
{
	context->RSSetViewports(1, &viewport);
}

inline RenderOutput::OutputState RenderOutput::CaptureOutput(ID3D11DeviceContext* context)
{
	OutputState state;
	context->OMGetRenderTargets(1, state.color.GetAddressOf(), state.depth.GetAddressOf());
	state.viewportCount = static_cast<UINT>(state.viewports.size());
	context->RSGetViewports(&state.viewportCount, state.viewports.data());
	return state;
}

inline void RenderOutput::RestoreOutput(ID3D11DeviceContext* context, const OutputState& state)
{
	context->OMSetRenderTargets(1, state.color.GetAddressOf(), state.depth.Get());
	context->RSSetViewports(state.viewportCount, state.viewports.data());
}
