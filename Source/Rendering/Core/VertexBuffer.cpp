#include "Rendering/Core/VertexBuffer.h"

Gpu::VertexBuffer::VertexBuffer(ID3D11Device* device, const void* data, UINT size)
	: Buffer(device, BufferDesc(size, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE),
		  D3D11_SUBRESOURCE_DATA{data, 0, 0})
{
}
