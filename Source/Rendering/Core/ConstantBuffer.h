#pragma once
#include "Rendering/Core/Buffer.h"
#include <initializer_list>

namespace Gpu
{
class ConstantBuffer : public Buffer
{
public:
	// BufferDescを受け取るコンストラクタも使用する。
	using Buffer::Buffer;
	static Microsoft::WRL::ComPtr<ID3D11Buffer> GetBoundPS(ID3D11DeviceContext* context, UINT slot);
	ConstantBuffer(ID3D11Device* device, UINT size);
	void BindVS(ID3D11DeviceContext* context, UINT slot) const { context->VSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindVS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindVS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativeVS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->VSSetConstantBuffers(slot, 1, &buffer); }
	void BindPS(ID3D11DeviceContext* context, UINT slot) const { context->PSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindPS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindPS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativePS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->PSSetConstantBuffers(slot, 1, &buffer); }
	void BindGS(ID3D11DeviceContext* context, UINT slot) const { context->GSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindGS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindGS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativeGS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->GSSetConstantBuffers(slot, 1, &buffer); }
	void BindHS(ID3D11DeviceContext* context, UINT slot) const { context->HSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindHS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindHS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativeHS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->HSSetConstantBuffers(slot, 1, &buffer); }
	void BindDS(ID3D11DeviceContext* context, UINT slot) const { context->DSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindDS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindDS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativeDS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->DSSetConstantBuffers(slot, 1, &buffer); }
	void BindCS(ID3D11DeviceContext* context, UINT slot) const { context->CSSetConstantBuffers(slot, 1, GetAddressOf()); }
	static void BindCS(ID3D11DeviceContext* context, UINT firstSlot, std::initializer_list<const ConstantBuffer*> buffers);
	static void UnbindCS(ID3D11DeviceContext* context, UINT firstSlot, UINT count = 1);
	static void BindNativeCS(ID3D11DeviceContext* context, UINT slot, ID3D11Buffer* buffer) { context->CSSetConstantBuffers(slot, 1, &buffer); }
};
}