#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <stdexcept>
#include <utility>

namespace Gpu
{
class RasterizerState
{
  public:
	RasterizerState(ID3D11Device* device, const D3D11_RASTERIZER_DESC& description)
	{
		if (!device) throw std::invalid_argument("GPU device is null.");
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> replacement;
		if (FAILED(device->CreateRasterizerState(&description, replacement.GetAddressOf())))
			throw std::runtime_error("CreateRasterizerState failed.");
		state = std::move(replacement);
	}
	RasterizerState(ID3D11Device* device, D3D11_FILL_MODE fill, D3D11_CULL_MODE cull,
		bool antialiasedLines = false)
		: RasterizerState(device, MakeDescription(fill, cull, antialiasedLines))
	{
	}
	void Bind(ID3D11DeviceContext* context) const
	{
		context->RSSetState(state.Get());
	}
	ID3D11RasterizerState* Get() const
	{
		return state.Get();
	}

	static Microsoft::WRL::ComPtr<ID3D11RasterizerState> Capture(ID3D11DeviceContext* context)
	{
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> state;
		context->RSGetState(state.GetAddressOf());
		return state;
	}
	static void Restore(ID3D11DeviceContext* context, ID3D11RasterizerState* state)
	{
		context->RSSetState(state);
	}

  private:
	static D3D11_RASTERIZER_DESC MakeDescription(
		D3D11_FILL_MODE fill, D3D11_CULL_MODE cull, bool antialiasedLines)
	{
		D3D11_RASTERIZER_DESC description{};
		description.FillMode = fill;
		description.CullMode = cull;
		description.DepthClipEnable = true;
		description.MultisampleEnable = true;
		description.AntialiasedLineEnable = antialiasedLines;
		return description;
	}
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> state;
};
} // namespace Gpu
