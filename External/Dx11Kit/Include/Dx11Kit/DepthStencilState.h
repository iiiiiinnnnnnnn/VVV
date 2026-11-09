#pragma once

#include <Dx11Kit/Common.h>


namespace Gpu
{
class DepthStencilState
{
  public:
	DepthStencilState(ID3D11Device* device, const D3D11_DEPTH_STENCIL_DESC& description)
	{
		if (!device) throw std::invalid_argument("GPU device is null.");
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> replacement;
		if (FAILED(device->CreateDepthStencilState(&description, replacement.GetAddressOf())))
			throw std::runtime_error("CreateDepthStencilState failed.");
		state = std::move(replacement);
	}
	DepthStencilState(ID3D11Device* device, bool test, bool write,
		D3D11_COMPARISON_FUNC comparison = D3D11_COMPARISON_LESS_EQUAL)
		: DepthStencilState(device, MakeDescription(test, write, comparison))
	{
	}
	
	void Bind(ID3D11DeviceContext* context, UINT stencilReference = 0) const
	{
		context->OMSetDepthStencilState(state.Get(), stencilReference);
	}
	ID3D11DepthStencilState* Get() const
	{
		return state.Get();
	}

	struct Binding
	{
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state;
		UINT stencilReference = 0;
	};
	static Binding Capture(ID3D11DeviceContext* context)
	{
		Binding binding;
		context->OMGetDepthStencilState(binding.state.GetAddressOf(), &binding.stencilReference);
		return binding;
	}
	static void Restore(ID3D11DeviceContext* context, const Binding& binding)
	{
		context->OMSetDepthStencilState(binding.state.Get(), binding.stencilReference);
	}

  private:
	// DepthStencilStateの説明を作成するユーティリティ関数
	static D3D11_DEPTH_STENCIL_DESC MakeDescription(
		bool test, bool write, D3D11_COMPARISON_FUNC comparison)
	{
		D3D11_DEPTH_STENCIL_DESC description{};
		description.DepthEnable = test;
		description.DepthWriteMask =
			write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		description.DepthFunc = comparison;
		return description;
	}
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> state;
};
} // namespace Gpu
