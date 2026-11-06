#include "Rendering/Core/VertexShader.h"
#include "Rendering/Core/ShaderBytecode.h"

#include <stdexcept>
#include <array>
#include <utility>

Gpu::VertexShader::VertexShader(ID3D11Device* device, const char* filename,
	const InputLayoutDesc& layout)
{
	ShaderBytecode bytecode;
	bytecode.Load(filename);
	Create(device, bytecode.GetData(), bytecode.GetSize(), layout);
}

Gpu::VertexShader::VertexShader(ID3D11Device* device, const void* bytecode, size_t size,
	const InputLayoutDesc& layout)
{
	Create(device, bytecode, size, layout);
}

void Gpu::VertexShader::Create(ID3D11Device* device, const void* bytecode, size_t size,
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

void Gpu::VertexShader::Bind(ID3D11DeviceContext* context) const
{
	context->VSSetShader(shader.Get(), nullptr, 0);
	context->IASetInputLayout(inputLayout.Get());
}

void Gpu::VertexShader::Unbind(ID3D11DeviceContext* context)
{
	context->VSSetShader(nullptr, nullptr, 0);
}

void Gpu::VertexShader::SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
	std::span<ID3D11ShaderResourceView* const> resources)
{
	constexpr UINT limit = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	if (firstSlot > limit || resources.size() > limit - firstSlot) throw std::out_of_range("Shader resource slots.");
	context->VSSetShaderResources(firstSlot, static_cast<UINT>(resources.size()), resources.data());
}

void Gpu::VertexShader::SetShaderResources(ID3D11DeviceContext* context, UINT slot, ID3D11ShaderResourceView* resource)
{
	SetShaderResources(context, slot, std::span<ID3D11ShaderResourceView* const>(&resource, 1));
}

void Gpu::VertexShader::SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
	std::initializer_list<ID3D11ShaderResourceView*> resources)
{
	SetShaderResources(context, firstSlot, std::span<ID3D11ShaderResourceView* const>(resources.begin(), resources.size()));
}

void Gpu::VertexShader::UnbindShaderResources(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Shader resource slots.");
	std::array<ID3D11ShaderResourceView*, limit> resources{};
	SetShaderResources(context, firstSlot, std::span<ID3D11ShaderResourceView* const>(resources.data(), count));
}

void Gpu::VertexShader::Draw(ID3D11DeviceContext* context, UINT count, UINT start) const
{
	context->Draw(count, start);
}

void Gpu::VertexShader::UnbindInputLayout(ID3D11DeviceContext* context)
{
	context->IASetInputLayout(nullptr);
}
