#include "Rendering/Core/GeometryShader.h"
#include "Rendering/Core/ShaderBytecode.h"

#include <stdexcept>
#include <array>
#include <utility>

Gpu::GeometryShader::GeometryShader(ID3D11Device* device, const char* filename)
{
	ShaderBytecode bytecode;
	bytecode.Load(filename);
	Create(device, bytecode.GetData(), bytecode.GetSize());
}

Gpu::GeometryShader::GeometryShader(ID3D11Device* device, const void* bytecode, size_t size)
{
	Create(device, bytecode, size);
}

void Gpu::GeometryShader::Create(ID3D11Device* device, const void* bytecode, size_t size)
{
	if (!device || !bytecode || !size) throw std::invalid_argument("Invalid geometry shader source.");
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> replacement;
	if (FAILED(device->CreateGeometryShader(bytecode, size, nullptr, replacement.GetAddressOf())))
		throw std::runtime_error("Could not create geometry shader.");
	shader = std::move(replacement);
}

void Gpu::GeometryShader::Bind(ID3D11DeviceContext* context) const
{
	context->GSSetShader(shader.Get(), nullptr, 0);
}

void Gpu::GeometryShader::Unbind(ID3D11DeviceContext* context)
{
	context->GSSetShader(nullptr, nullptr, 0);
}

void Gpu::GeometryShader::SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
	std::span<ID3D11ShaderResourceView* const> resources)
{
	constexpr UINT limit = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	if (firstSlot > limit || resources.size() > limit - firstSlot) throw std::out_of_range("Shader resource slots.");
	context->GSSetShaderResources(firstSlot, static_cast<UINT>(resources.size()), resources.data());
}

void Gpu::GeometryShader::SetShaderResources(ID3D11DeviceContext* context, UINT slot, ID3D11ShaderResourceView* resource)
{
	SetShaderResources(context, slot, std::span<ID3D11ShaderResourceView* const>(&resource, 1));
}

void Gpu::GeometryShader::SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
	std::initializer_list<ID3D11ShaderResourceView*> resources)
{
	SetShaderResources(context, firstSlot, std::span<ID3D11ShaderResourceView* const>(resources.begin(), resources.size()));
}

void Gpu::GeometryShader::UnbindShaderResources(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Shader resource slots.");
	std::array<ID3D11ShaderResourceView*, limit> resources{};
	SetShaderResources(context, firstSlot, std::span<ID3D11ShaderResourceView* const>(resources.data(), count));
}
