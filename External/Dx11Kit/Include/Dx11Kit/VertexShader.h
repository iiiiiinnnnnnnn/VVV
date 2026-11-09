#pragma once

#include <Dx11Kit/Common.h>
#include <Dx11Kit/ShaderBytecode.h>

#include <Dx11Kit/InputLayoutDesc.h>

namespace Gpu
{
class VertexShader
{
  public:
	VertexShader(ID3D11Device* device, const char* filename, const InputLayoutDesc& layout = {});
	VertexShader(ID3D11Device* device, const void* bytecode, size_t size,
		const InputLayoutDesc& layout = {});

	// シェーダーリソース設定用
	static void SetShaderResources(
		ID3D11DeviceContext* context, UINT slot, ID3D11ShaderResourceView* resource);
	static void SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
		std::initializer_list<ID3D11ShaderResourceView*> resources);
	static void SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
		std::span<ID3D11ShaderResourceView* const> resources);
	static void UnbindShaderResources(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);

	void Draw(ID3D11DeviceContext* context, UINT count, UINT start = 0) const;
	static void UnbindInputLayout(ID3D11DeviceContext* context);
	void Bind(ID3D11DeviceContext* context) const;
	static void Unbind(ID3D11DeviceContext* context);
	
	ID3D11VertexShader* Get() const
	{
		return shader.Get();
	}
	ID3D11InputLayout* GetInputLayout() const
	{
		return inputLayout.Get();
	}

	operator bool() const
	{
		return shader.Get();
	}

  private:
	void Create(
		ID3D11Device* device, const void* bytecode, size_t size, const InputLayoutDesc& layout);
	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
};
} // namespace Gpu

inline Gpu::VertexShader::VertexShader(ID3D11Device* device, const char* filename,
	const InputLayoutDesc& layout)
{
	ShaderBytecode bytecode;
	bytecode.Load(filename);
	Create(device, bytecode.GetData(), bytecode.GetSize(), layout);
}

inline Gpu::VertexShader::VertexShader(ID3D11Device* device, const void* bytecode, size_t size,
	const InputLayoutDesc& layout)
{
	Create(device, bytecode, size, layout);
}

inline void Gpu::VertexShader::Create(ID3D11Device* device, const void* bytecode, size_t size,
	const InputLayoutDesc& layout)
{
	if (!device || !bytecode || !size) throw std::invalid_argument("Invalid vertex shader source.");
	const auto elements = layout.GetElements();
	Microsoft::WRL::ComPtr<ID3D11VertexShader> replacement;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> replacementLayout;
	if (FAILED(device->CreateVertexShader(bytecode, size, nullptr, replacement.GetAddressOf())))
		throw std::runtime_error("Could not create vertex shader.");
	if (layout.GetCount() && FAILED(device->CreateInputLayout(elements.data(), layout.GetCount(), bytecode, size, replacementLayout.GetAddressOf())))
		throw std::runtime_error("Could not create vertex input layout.");
	shader = std::move(replacement);
	inputLayout = std::move(replacementLayout);
}

inline void Gpu::VertexShader::Bind(ID3D11DeviceContext* context) const
{
	context->VSSetShader(shader.Get(), nullptr, 0);
	context->IASetInputLayout(inputLayout.Get());
}

inline void Gpu::VertexShader::Unbind(ID3D11DeviceContext* context)
{
	context->VSSetShader(nullptr, nullptr, 0);
}

inline void Gpu::VertexShader::SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
	std::span<ID3D11ShaderResourceView* const> resources)
{
	constexpr UINT limit = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	if (firstSlot > limit || resources.size() > limit - firstSlot) throw std::out_of_range("Shader resource slots.");
	context->VSSetShaderResources(firstSlot, static_cast<UINT>(resources.size()), resources.data());
}

inline void Gpu::VertexShader::SetShaderResources(ID3D11DeviceContext* context, UINT slot, ID3D11ShaderResourceView* resource)
{
	SetShaderResources(context, slot, std::span<ID3D11ShaderResourceView* const>(&resource, 1));
}

inline void Gpu::VertexShader::SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
	std::initializer_list<ID3D11ShaderResourceView*> resources)
{
	SetShaderResources(context, firstSlot, std::span<ID3D11ShaderResourceView* const>(resources.begin(), resources.size()));
}

inline void Gpu::VertexShader::UnbindShaderResources(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Shader resource slots.");
	std::array<ID3D11ShaderResourceView*, limit> resources{};
	SetShaderResources(context, firstSlot, std::span<ID3D11ShaderResourceView* const>(resources.data(), count));
}

inline void Gpu::VertexShader::Draw(ID3D11DeviceContext* context, UINT count, UINT start) const
{
	context->Draw(count, start);
}

inline void Gpu::VertexShader::UnbindInputLayout(ID3D11DeviceContext* context)
{
	context->IASetInputLayout(nullptr);
}
