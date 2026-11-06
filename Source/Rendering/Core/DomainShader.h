#pragma once

#include <cstddef>
#include <span>
#include <initializer_list>
#include <d3d11.h>
#include <wrl.h>

namespace Gpu
{
class DomainShader
{
  public:
	DomainShader(ID3D11Device* device, const char* filename);
	DomainShader(ID3D11Device* device, const void* bytecode, size_t size);
	
	void Bind(ID3D11DeviceContext* context) const;
	static void Unbind(ID3D11DeviceContext* context);

	// シェーダーリソース設定用
	static void SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
		std::span<ID3D11ShaderResourceView* const> resources);
	static void SetShaderResources(ID3D11DeviceContext* context, UINT slot, ID3D11ShaderResourceView* resource);
	static void SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
		std::initializer_list<ID3D11ShaderResourceView*> resources);
	static void UnbindShaderResources(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	
	ID3D11DomainShader* Get() const
	{
		return shader.Get();
	}
	operator bool() const
	{
		return shader.Get();
	}

  private:
	void Create(ID3D11Device* device, const void* bytecode, size_t size);
	Microsoft::WRL::ComPtr<ID3D11DomainShader> shader;
};
} // namespace Gpu

