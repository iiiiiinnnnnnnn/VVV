#pragma once

#include "Rendering/Core/InputLayoutDesc.h"
#include <cstddef>
#include <span>
#include <initializer_list>
#include <d3d11.h>
#include <wrl.h>

namespace Gpu
{
class VertexShader
{
  public:
	VertexShader(ID3D11Device* device, const char* filename, const InputLayoutDesc& layout = {});
	VertexShader(ID3D11Device* device, const void* bytecode, size_t size,
		const InputLayoutDesc& layout = {});

	// シェーダーリソース設定用
	static void SetShaderResources(
		ID3D11DeviceContext* context, UINT slot, ID3D11ShaderResourceView* resource);
	static void SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
		std::initializer_list<ID3D11ShaderResourceView*> resources);
	static void SetShaderResources(ID3D11DeviceContext* context, UINT firstSlot,
		std::span<ID3D11ShaderResourceView* const> resources);
	static void UnbindShaderResources(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);

	void Draw(ID3D11DeviceContext* context, UINT count, UINT start = 0) const;
	static void UnbindInputLayout(ID3D11DeviceContext* context);
	void Bind(ID3D11DeviceContext* context) const;
	static void Unbind(ID3D11DeviceContext* context);
	
	ID3D11VertexShader* Get() const
	{
		return shader.Get();
	}
	ID3D11InputLayout* GetInputLayout() const
	{
		return inputLayout.Get();
	}

	operator bool() const
	{
		return shader.Get();
	}

  private:
	void Create(
		ID3D11Device* device, const void* bytecode, size_t size, const InputLayoutDesc& layout);
	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
};
} // namespace Gpu
