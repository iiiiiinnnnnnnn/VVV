#pragma once

#include <Dx11Kit/Common.h>
#include <Dx11Kit/Buffer.h>

namespace Gpu
{
class ConstantBuffer : public Buffer
{
public:
	// BufferDescを受け取るコンストラクタも使用する。
	using Buffer::Buffer;
	static Microsoft::WRL::ComPtr<ID3D11Buffer> GetBoundPS(ID3D11DeviceContext* context, UINT slot);
	ConstantBuffer(ID3D11Device* device, UINT size);
	void BindVS(ID3D11DeviceContext* context, UINT slot) const { context->VSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindVS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindVS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativeVS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->VSSetConstantBuffers(slot, 1, &buffer); }
	void BindPS(ID3D11DeviceContext* context, UINT slot) const { context->PSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindPS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindPS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativePS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->PSSetConstantBuffers(slot, 1, &buffer); }
	void BindGS(ID3D11DeviceContext* context, UINT slot) const { context->GSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindGS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindGS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativeGS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->GSSetConstantBuffers(slot, 1, &buffer); }
	void BindHS(ID3D11DeviceContext* context, UINT slot) const { context->HSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindHS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindHS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativeHS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->HSSetConstantBuffers(slot, 1, &buffer); }
	void BindDS(ID3D11DeviceContext* context, UINT slot) const { context->DSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindDS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindDS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativeDS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->DSSetConstantBuffers(slot, 1, &buffer); }
	void BindCS(ID3D11DeviceContext* context, UINT slot) const { context->CSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindCS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindCS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativeCS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->CSSetConstantBuffers(slot, 1, &buffer); }
};
}
inline Gpu::ConstantBuffer::ConstantBuffer(ID3D11Device* device, UINT size)
	: Buffer(device, BufferDesc(size, D3D11_BIND_CONSTANT_BUFFER))
{
}

inline void Gpu::ConstantBuffer::BindVS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->VSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

inline void Gpu::ConstantBuffer::UnbindVS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->VSSetConstantBuffers(firstSlot, count, values.data());
}

inline void Gpu::ConstantBuffer::BindPS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->PSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

inline void Gpu::ConstantBuffer::UnbindPS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->PSSetConstantBuffers(firstSlot, count, values.data());
}

inline void Gpu::ConstantBuffer::BindGS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->GSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

inline void Gpu::ConstantBuffer::UnbindGS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->GSSetConstantBuffers(firstSlot, count, values.data());
}

inline void Gpu::ConstantBuffer::BindHS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->HSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

inline void Gpu::ConstantBuffer::UnbindHS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->HSSetConstantBuffers(firstSlot, count, values.data());
}

inline void Gpu::ConstantBuffer::BindDS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->DSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

inline void Gpu::ConstantBuffer::UnbindDS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->DSSetConstantBuffers(firstSlot, count, values.data());
}

inline void Gpu::ConstantBuffer::BindCS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || buffers.size() > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	size_t i = 0;
	for (const auto* value : buffers) values[i++] = value ? value->Get() : nullptr;
	context->CSSetConstantBuffers(firstSlot, static_cast<UINT>(buffers.size()), values.data());
}

inline void Gpu::ConstantBuffer::UnbindCS(ID3D11DeviceContext* context, UINT firstSlot, UINT count)
{
	constexpr UINT limit = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
	if (firstSlot > limit || count > limit - firstSlot) throw std::out_of_range("Constant buffer slots.");
	std::array<ID3D11Buffer*, limit> values{};
	context->CSSetConstantBuffers(firstSlot, count, values.data());
}

inline Microsoft::WRL::ComPtr<ID3D11Buffer> Gpu::ConstantBuffer::GetBoundPS(ID3D11DeviceContext* context, UINT slot)
{
	if (slot >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) throw std::out_of_range("Constant buffer slot.");
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	context->PSGetConstantBuffers(slot, 1, buffer.GetAddressOf());
	return buffer;
}
