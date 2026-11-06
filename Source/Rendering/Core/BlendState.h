#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <stdexcept>
#include <utility>
#include <array>

namespace Gpu
{
class BlendState
{
  public:
	BlendState(ID3D11Device* device, const D3D11_BLEND_DESC& description)
	{
		Microsoft::WRL::ComPtr<ID3D11BlendState> replacement;
		if (FAILED(device->CreateBlendState(&description, replacement.GetAddressOf())))
			throw std::runtime_error("CreateBlendState failed.");
		state = std::move(replacement);
	}
	BlendState(ID3D11Device* device, bool enabled, D3D11_BLEND source, D3D11_BLEND destination,
		D3D11_BLEND_OP operation = D3D11_BLEND_OP_ADD)
		: BlendState(device, MakeDescription(enabled, source, destination, operation))
	{
	}
	
	void Bind(ID3D11DeviceContext* context, const float* factor = nullptr,
		UINT sampleMask = 0xFFFFFFFF) const
	{
		context->OMSetBlendState(state.Get(), factor, sampleMask);
	}
	ID3D11BlendState* Get() const
	{
		return state.Get();
	}

	struct Binding
	{
		Microsoft::WRL::ComPtr<ID3D11BlendState> state;
		std::array<float, 4> factor{};
		UINT sampleMask = 0xFFFFFFFF;
	};
	static Binding Capture(ID3D11DeviceContext* context)
	{
		Binding binding;
		context->OMGetBlendState(binding.state.GetAddressOf(), binding.factor.data(), &binding.sampleMask);
		return binding;
	}
	static void Restore(ID3D11DeviceContext* context, const Binding& binding)
	{
		context->OMSetBlendState(binding.state.Get(), binding.factor.data(), binding.sampleMask);
	}
	static void Unbind(ID3D11DeviceContext* context)
	{
		context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	}

  private:
	// BlendStateの説明を作成するユーティリティ関数
	static D3D11_BLEND_DESC MakeDescription(
		bool enabled, D3D11_BLEND source, D3D11_BLEND destination, D3D11_BLEND_OP operation)
	{
		D3D11_BLEND_DESC description{};
		auto& target = description.RenderTarget[0];
		target.BlendEnable = enabled;
		target.SrcBlend = source;
		target.DestBlend = destination;
		target.BlendOp = operation;
		target.SrcBlendAlpha = D3D11_BLEND_ONE;
		target.DestBlendAlpha = D3D11_BLEND_ZERO;
		target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		return description;
	}
	Microsoft::WRL::ComPtr<ID3D11BlendState> state;
};
} // namespace Gpu
