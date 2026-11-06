#pragma once
#include <d3d11.h>

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
