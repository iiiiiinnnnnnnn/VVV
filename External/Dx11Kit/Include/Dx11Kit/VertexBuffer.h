#pragma once

#include <Dx11Kit/Common.h>
#include <Dx11Kit/Buffer.h>

namespace Gpu
{
class VertexBuffer : public Buffer
{
  public:
	// BufferDescを受け取るコンストラクタも使用する。
	using Buffer::Buffer;
	VertexBuffer(ID3D11Device* device, const void* data, UINT size);
	void Bind(ID3D11DeviceContext* context, UINT stride, UINT slot = 0, UINT offset = 0) const
	{
		context->IASetVertexBuffers(slot, 1, GetAddressOf(), &stride, &offset);
	}
	void Draw(ID3D11DeviceContext* context, UINT count, UINT start = 0) const
	{
		context->Draw(count, start);
	}
	static void SetTopology(ID3D11DeviceContext* context, D3D11_PRIMITIVE_TOPOLOGY topology)
	{
		context->IASetPrimitiveTopology(topology);
	}
	static void Unbind(ID3D11DeviceContext* context, UINT slot = 0)
	{
		ID3D11Buffer* buffer = nullptr;
		UINT stride = 0, offset = 0;
		context->IASetVertexBuffers(slot, 1, &buffer, &stride, &offset);
	}
};
} // namespace Gpu

inline Gpu::VertexBuffer::VertexBuffer(ID3D11Device* device, const void* data, UINT size)
	: Buffer(device, BufferDesc(size, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE),
		  D3D11_SUBRESOURCE_DATA{data, 0, 0})
{
}
