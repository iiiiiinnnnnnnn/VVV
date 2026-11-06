#pragma once

#include "Rendering/Core/BufferDesc.h"
#include <wrl.h>

namespace Gpu
{
class Buffer
{
  public:
	Buffer(ID3D11Device* device, const BufferDesc& description,
		const D3D11_SUBRESOURCE_DATA* initialData = nullptr);
	Buffer(ID3D11Device* device, const BufferDesc& description,
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