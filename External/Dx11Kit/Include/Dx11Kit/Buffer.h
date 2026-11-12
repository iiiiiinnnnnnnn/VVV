#pragma once

#include <Dx11Kit/Common.h>

namespace Gpu
{
class Buffer
{
  public:
	Buffer(ID3D11Device* device, const D3D11_BUFFER_DESC& description,
		const D3D11_SUBRESOURCE_DATA* initialData = nullptr);
	Buffer(ID3D11Device* device, const D3D11_BUFFER_DESC& description,
		const D3D11_SUBRESOURCE_DATA& initialData);

	void Update(ID3D11DeviceContext* context, const void* data) const;
	void Update(ID3D11DeviceContext* context, const void* data, UINT size) const;

	ID3D11Buffer* Get() const
	{
		return buffer.Get();
	}
	ID3D11Buffer* const* GetAddressOf() const
	{
		return buffer.GetAddressOf();
	}
	UINT GetByteWidth() const
	{
		return byteWidth;
	}

	operator bool() const
	{
		return buffer.Get();
	}

  private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	UINT byteWidth = 0;
	UINT bindFlags = 0;
	D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
};
} // namespace Gpu
inline Gpu::Buffer::Buffer(ID3D11Device* device, const D3D11_BUFFER_DESC& description,
	const D3D11_SUBRESOURCE_DATA* initialData)
	: byteWidth(description.ByteWidth), bindFlags(description.BindFlags), usage(description.Usage)
{
	if (!device || !byteWidth) throw std::invalid_argument("Invalid GPU buffer description.");
	if ((bindFlags & D3D11_BIND_CONSTANT_BUFFER) && (byteWidth & 15))
		throw std::invalid_argument("Constant buffer size must be a multiple of 16.");
	if (FAILED(device->CreateBuffer(&description, initialData, buffer.GetAddressOf())))
		throw std::runtime_error("Could not create GPU buffer.");
}

inline Gpu::Buffer::Buffer(ID3D11Device* device, const D3D11_BUFFER_DESC& description,
	const D3D11_SUBRESOURCE_DATA& initialData)
	: Buffer(device, description, &initialData)
{
}

inline void Gpu::Buffer::Update(ID3D11DeviceContext* context, const void* data) const
{
	Update(context, data, byteWidth);
}

inline void Gpu::Buffer::Update(ID3D11DeviceContext* context, const void* data, UINT size) const
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

