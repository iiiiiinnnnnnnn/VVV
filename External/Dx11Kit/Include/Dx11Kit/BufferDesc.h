#pragma once

#include <Dx11Kit/Common.h>

namespace Gpu
{
// バッファの作成設定。値として保持し、GPUリソースは持たない
class BufferDesc
{
  public:
	BufferDesc(UINT size, UINT bindFlags, D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
		UINT cpuAccessFlags = 0, UINT miscFlags = 0, UINT structureByteStride = 0);

	UINT GetByteWidth() const
	{
		return description.ByteWidth;
	}
	UINT GetBindFlags() const
	{
		return description.BindFlags;
	}
	D3D11_USAGE GetUsage() const
	{
		return description.Usage;
	}
	const D3D11_BUFFER_DESC& Get() const
	{
		return description;
	}

  private:
	D3D11_BUFFER_DESC description{};
};
} // namespace Gpu

inline Gpu::BufferDesc::BufferDesc(UINT size, UINT bindFlags, D3D11_USAGE usage,
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
