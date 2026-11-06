#include "Rendering/Core/ConstantBuffer.h"
#include <stdexcept>
#include <array>

Gpu::ConstantBuffer::ConstantBuffer(ID3D11Device* device, UINT size)
	: Buffer(device, BufferDesc(size, D3D11_BIND_CONSTANT_BUFFER))
{
}

void Gpu::ConstantBuffer::BindVS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->VSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

void Gpu::ConstantBuffer::UnbindVS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->VSSetConstantBuffers(firstSlot, count, values.data());
}

void Gpu::ConstantBuffer::BindPS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->PSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

void Gpu::ConstantBuffer::UnbindPS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->PSSetConstantBuffers(firstSlot, count, values.data());
}

void Gpu::ConstantBuffer::BindGS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->GSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

void Gpu::ConstantBuffer::UnbindGS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->GSSetConstantBuffers(firstSlot, count, values.data());
}

void Gpu::ConstantBuffer::BindHS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->HSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

void Gpu::ConstantBuffer::UnbindHS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->HSSetConstantBuffers(firstSlot, count, values.data());
}

void Gpu::ConstantBuffer::BindDS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->DSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

void Gpu::ConstantBuffer::UnbindDS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->DSSetConstantBuffers(firstSlot, count, values.data());
}

void Gpu::ConstantBuffer::BindCS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->CSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

void Gpu::ConstantBuffer::UnbindCS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->CSSetConstantBuffers(firstSlot, count, values.data());
}

Microsoft::WRL::ComPtr<ID3D11Buffer> Gpu::ConstantBuffer::GetBoundPS(ID3D11DeviceContext* context, UINT slot)
{
	if (slot >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) throw std::out_of_range("Constant buffer slot.");
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	context->PSGetConstantBuffers(slot, 1, buffer.GetAddressOf());
	return buffer;
}
