#pragma once

#include <Dx11Kit/Common.h>
#include <Dx11Kit/Buffer.h>

namespace Gpu
{
class IndexBuffer : public Buffer
{
  public:
	using Buffer::Buffer;
	IndexBuffer(ID3D11Device* device, const void* data, UINT size);
	
	void Draw(ID3D11DeviceContext* context, UINT count, UINT start = 0, INT baseVertex = 0) const
	{
		context->DrawIndexed(count, start, baseVertex);
	}
	static void Unbind(ID3D11DeviceContext* context, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN)
	{
		context->IASetIndexBuffer(nullptr, format, 0);
	}
	void Bind(ID3D11DeviceContext* context, DXGI_FORMAT format = DXGI_FORMAT_R32_UINT,
		UINT offset = 0) const
	{
		context->IASetIndexBuffer(Get(), format, offset);
	}
};
} // namespace Gpu


inline Gpu::IndexBuffer::IndexBuffer(ID3D11Device* device, const void* data, UINT size)
	: Buffer(device, D3D11_BUFFER_DESC{size, D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER, 0, 0, 0}, D3D11_SUBRESOURCE_DATA{data, 0, 0})
{
}

