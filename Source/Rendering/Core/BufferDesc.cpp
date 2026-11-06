#include "Rendering/Core/BufferDesc.h"
#include <stdexcept>

Gpu::BufferDesc::BufferDesc(UINT size, UINT bindFlags, D3D11_USAGE usage,
	UINT cpuAccessFlags, UINT miscFlags, UINT structureByteStride)
{
	if (!size) throw std::invalid_argument("Buffer size must be greater than zero.");
	if ((bindFlags & D3D11_BIND_CONSTANT_BUFFER) && (size & 15))
		throw std::invalid_argument("Constant buffer size must be a multiple of 16.");
	description.ByteWidth = size;
	description.BindFlags = bindFlags;
	description.Usage = usage;
	description.CPUAccessFlags = cpuAccessFlags;
	description.MiscFlags = miscFlags;
	description.StructureByteStride = structureByteStride;
}
