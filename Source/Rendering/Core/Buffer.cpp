#include "Rendering/Core/Buffer.h"

#include <cstring>
#include <stdexcept>

Gpu::Buffer::Buffer(ID3D11Device* device, const BufferDesc& description,
	const D3D11_SUBRESOURCE_DATA* initialData)
	: byteWidth(description.GetByteWidth()), bindFlags(description.GetBindFlags()), usage(description.GetUsage())
{
	if (!device || !byteWidth) throw std::invalid_argument("Invalid GPU buffer description.");
	if (FAILED(device->CreateBuffer(&description.Get(), initialData, buffer.GetAddressOf())))
		throw std::runtime_error("Could not create GPU buffer.");
}

Gpu::Buffer::Buffer(ID3D11Device* device, const BufferDesc& description,
	const D3D11_SUBRESOURCE_DATA& initialData)
	: Buffer(device, description, &initialData)
{
}

void Gpu::Buffer::Update(ID3D11DeviceContext* context, const void* data) const
{
	Update(context, data, byteWidth);
}

void Gpu::Buffer::Update(ID3D11DeviceContext* context, const void* data, UINT size) const
{
	if (!context || !buffer || !data || !size || size > byteWidth)
		throw std::invalid_argument("Invalid GPU buffer update.");
	if ((bindFlags & D3D11_BIND_CONSTANT_BUFFER) && size != byteWidth)
		throw std::invalid_argument("Constant buffers require a complete update.");
	if (usage == D3D11_USAGE_DYNAMIC)
	{
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			throw std::runtime_error("Could not map GPU buffer.");
		std::memcpy(mapped.pData, data, size);
		context->Unmap(buffer.Get(), 0);
	}
	else if (usage == D3D11_USAGE_DEFAULT)
	{
		D3D11_BOX range{0, 0, 0, size, 1, 1};
		context->UpdateSubresource(buffer.Get(), 0, size == byteWidth ? nullptr : &range, data, 0, 0);
	}
	else throw std::logic_error("This GPU buffer cannot be updated.");
}

