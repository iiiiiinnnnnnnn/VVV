#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <stdexcept>
#include <utility>
#include <array>
#include <initializer_list>

namespace Gpu
{
class SamplerState
{
  public:
	SamplerState(ID3D11Device* device, const D3D11_SAMPLER_DESC& description)
	{
		Microsoft::WRL::ComPtr<ID3D11SamplerState> replacement;
		if (FAILED(device->CreateSamplerState(&description, replacement.GetAddressOf())))
			throw std::runtime_error("CreateSamplerState failed.");
		state = std::move(replacement);
	}
	SamplerState(ID3D11Device* device, D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE address,
		UINT anisotropy = 1, float minimumLod = -D3D11_FLOAT32_MAX,
		float maximumLod = D3D11_FLOAT32_MAX)
		: SamplerState(device, MakeDescription(filter, address, anisotropy, minimumLod, maximumLod))
	{
	}
	void BindVS(ID3D11DeviceContext* context, UINT slot) const
	{
		ID3D11SamplerState* value = state.Get();
		context->VSSetSamplers(slot, 1, &value);
	}
	void BindPS(ID3D11DeviceContext* context, UINT slot) const
	{
		ID3D11SamplerState* value = state.Get();
		context->PSSetSamplers(slot, 1, &value);
	}
	void BindGS(ID3D11DeviceContext* context, UINT slot) const
	{
		ID3D11SamplerState* value = state.Get();
		context->GSSetSamplers(slot, 1, &value);
	}
	void BindHS(ID3D11DeviceContext* context, UINT slot) const
	{
		ID3D11SamplerState* value = state.Get();
		context->HSSetSamplers(slot, 1, &value);
	}
	void BindDS(ID3D11DeviceContext* context, UINT slot) const
	{
		ID3D11SamplerState* value = state.Get();
		context->DSSetSamplers(slot, 1, &value);
	}
	void BindCS(ID3D11DeviceContext* context, UINT slot) const
	{
		ID3D11SamplerState* value = state.Get();
		context->CSSetSamplers(slot, 1, &value);
	}
	static void BindVS(
		ID3D11DeviceContext* context, UINT firstSlot, UINT count, const SamplerState* const* states)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		for (UINT i = 0; i < count; ++i) values[i] = states[i] ? states[i]->Get() : nullptr;
		context->VSSetSamplers(firstSlot, count, values.data());
	}
	static void BindVS(ID3D11DeviceContext* context, UINT firstSlot,
		std::initializer_list<const SamplerState*> states)
	{
		BindVS(context, firstSlot, static_cast<UINT>(states.size()), states.begin());
	}
	static void UnbindVS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		context->VSSetSamplers(firstSlot, count, values.data());
	}
	static void BindPS(
		ID3D11DeviceContext* context, UINT firstSlot, UINT count, const SamplerState* const* states)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		for (UINT i = 0; i < count; ++i) values[i] = states[i] ? states[i]->Get() : nullptr;
		context->PSSetSamplers(firstSlot, count, values.data());
	}
	static void BindPS(ID3D11DeviceContext* context, UINT firstSlot,
		std::initializer_list<const SamplerState*> states)
	{
		BindPS(context, firstSlot, static_cast<UINT>(states.size()), states.begin());
	}
	static void UnbindPS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		context->PSSetSamplers(firstSlot, count, values.data());
	}
	static void BindGS(
		ID3D11DeviceContext* context, UINT firstSlot, UINT count, const SamplerState* const* states)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		for (UINT i = 0; i < count; ++i) values[i] = states[i] ? states[i]->Get() : nullptr;
		context->GSSetSamplers(firstSlot, count, values.data());
	}
	static void BindGS(ID3D11DeviceContext* context, UINT firstSlot,
		std::initializer_list<const SamplerState*> states)
	{
		BindGS(context, firstSlot, static_cast<UINT>(states.size()), states.begin());
	}
	static void UnbindGS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		context->GSSetSamplers(firstSlot, count, values.data());
	}
	static void BindHS(
		ID3D11DeviceContext* context, UINT firstSlot, UINT count, const SamplerState* const* states)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		for (UINT i = 0; i < count; ++i) values[i] = states[i] ? states[i]->Get() : nullptr;
		context->HSSetSamplers(firstSlot, count, values.data());
	}
	static void BindHS(ID3D11DeviceContext* context, UINT firstSlot,
		std::initializer_list<const SamplerState*> states)
	{
		BindHS(context, firstSlot, static_cast<UINT>(states.size()), states.begin());
	}
	static void UnbindHS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		context->HSSetSamplers(firstSlot, count, values.data());
	}
	static void BindDS(
		ID3D11DeviceContext* context, UINT firstSlot, UINT count, const SamplerState* const* states)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		for (UINT i = 0; i < count; ++i) values[i] = states[i] ? states[i]->Get() : nullptr;
		context->DSSetSamplers(firstSlot, count, values.data());
	}
	static void BindDS(ID3D11DeviceContext* context, UINT firstSlot,
		std::initializer_list<const SamplerState*> states)
	{
		BindDS(context, firstSlot, static_cast<UINT>(states.size()), states.begin());
	}
	static void UnbindDS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		context->DSSetSamplers(firstSlot, count, values.data());
	}
	static void BindCS(
		ID3D11DeviceContext* context, UINT firstSlot, UINT count, const SamplerState* const* states)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		for (UINT i = 0; i < count; ++i) values[i] = states[i] ? states[i]->Get() : nullptr;
		context->CSSetSamplers(firstSlot, count, values.data());
	}
	static void BindCS(ID3D11DeviceContext* context, UINT firstSlot,
		std::initializer_list<const SamplerState*> states)
	{
		BindCS(context, firstSlot, static_cast<UINT>(states.size()), states.begin());
	}
	static void UnbindCS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1)
	{
		constexpr UINT limit = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
		if (firstSlot > limit || count > limit - firstSlot)
			throw std::out_of_range("Sampler slots.");
		std::array<ID3D11SamplerState*, limit> values{};
		context->CSSetSamplers(firstSlot, count, values.data());
	}
	ID3D11SamplerState* Get() const
	{
		return state.Get();
	}

  private:
	static D3D11_SAMPLER_DESC MakeDescription(D3D11_FILTER filter,
		D3D11_TEXTURE_ADDRESS_MODE address, UINT anisotropy, float minimumLod, float maximumLod)
	{
		D3D11_SAMPLER_DESC description{};
		description.Filter = filter;
		description.AddressU = description.AddressV = description.AddressW = address;
		description.MaxAnisotropy = anisotropy;
		description.ComparisonFunc = D3D11_COMPARISON_NEVER;
		description.MinLOD = minimumLod;
		description.MaxLOD = maximumLod;
		for (float& channel : description.BorderColor) channel = 1.0f;
		return description;
	}
	Microsoft::WRL::ComPtr<ID3D11SamplerState> state;
};
} // namespace Gpu
