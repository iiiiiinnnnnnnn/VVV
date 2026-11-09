#pragma once

#include <Dx11Kit/Common.h>
#include <Dx11Kit/ShaderBytecode.h>


namespace Gpu
{
class PixelShader
{
  public:
	PixelShader(ID3D11Device* device, const char* filename);
	PixelShader(ID3D11Device* device, const void* bytecode, size_t size);
	
	void Bind(ID3D11DeviceContext* context) const;
	static void Unbind(ID3D11DeviceContext* context);

	// シェーダーリソース設定用
	static void SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
		std::span<ID3D11ShaderResourceView* const> resources);
	static void SetShaderResources(ID3D11DeviceContext* context, UINT slot, ID3D11ShaderResourceView* resource);
	static void SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
		std::initializer_list<ID3D11ShaderResourceView*> resources);
	static void UnbindShaderResources(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);

	ID3D11PixelShader* Get() const
	{
		return shader.Get();
	}
	operator bool() const
	{
		return shader.Get();
	}

  private:
	void Create(ID3D11Device* device, const void* bytecode, size_t size);
	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
};
} // namespace Gpu

inline Gpu::PixelShader::PixelShader(ID3D11Device* device, const char* filename)
{
	ShaderBytecode bytecode;
	bytecode.Load(filename);
	Create(device, bytecode.GetData(), bytecode.GetSize());
}

inline Gpu::PixelShader::PixelShader(ID3D11Device* device, const void* bytecode, size_t size)
{
	Create(device, bytecode, size);
}

inline void Gpu::PixelShader::Create(ID3D11Device* device, const void* bytecode, size_t size)
{
	if (!device || !bytecode || !size) throw std::invalid_argument("Invalid pixel shader source.");
	Microsoft::WRL::ComPtr<ID3D11PixelShader> replacement;
	if (FAILED(device->CreatePixelShader(bytecode, size, nullptr, replacement.GetAddressOf())))
		throw std::runtime_error("Could not create pixel shader.");
	shader = std::move(replacement);
}

inline void Gpu::PixelShader::Bind(ID3D11DeviceContext* context) const
{
	context->PSSetShader(shader.Get(), nullptr, 0);
}

inline void Gpu::PixelShader::Unbind(ID3D11DeviceContext* context)
{
	context->PSSetShader(nullptr, nullptr, 0);
}

inline void Gpu::PixelShader::SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
	std::span<ID3D11ShaderResourceView* const> resources)
{
	constexpr UINT limit = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	if (firstSlot > limit || resources.size() > limit - firstSlot) throw std::out_of_range("Shader resource slots.");
	context->PSSetShaderResources(firstSlot, static_cast<UINT>(resources.size()), resources.data());
}

inline void Gpu::PixelShader::SetShaderResources(ID3D11DeviceContext* context, UINT slot, ID3D11ShaderResourceView* resource)
{
	SetShaderResources(context, slot, std::span<ID3D11ShaderResourceView* const>(&resource, 1));
}

inline void Gpu::PixelShader::SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
	std::initializer_list<ID3D11ShaderResourceView*> resources)
{
	SetShaderResources(context, firstSlot, std::span<ID3D11ShaderResourceView* const>(resources.begin(), resources.size()));
}

inline void Gpu::PixelShader::UnbindShaderResources(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Shader resource slots.");
	std::array<ID3D11ShaderResourceView*, limit> resources{};
	SetShaderResources(context, firstSlot, std::span<ID3D11ShaderResourceView* const>(resources.data(), count));
}
