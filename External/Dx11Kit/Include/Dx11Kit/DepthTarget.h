#pragma once

#include <Dx11Kit/Common.h>

class DepthTarget
{
  public:
	DepthTarget(ID3D11Device* device, UINT width, UINT height,
		DXGI_FORMAT textureFormat, DXGI_FORMAT depthFormat, DXGI_FORMAT shaderFormat);

	void Clear(ID3D11DeviceContext* dc, UINT flags, float depth, UINT8 stencil) const;
	ID3D11DepthStencilView* GetDSV(bool readOnly) const { return readOnly ? readOnlyDsv.Get() : dsv.Get(); }
	ID3D11ShaderResourceView* GetSRV() const { return srv.Get(); }
	UINT GetWidth() const { return width; }
	UINT GetHeight() const { return height; }

  private:
	static void CheckCreation(HRESULT hr);
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> readOnlyDsv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	UINT width;
	UINT height;
};

inline void DepthTarget::CheckCreation(HRESULT hr)
{
	if (FAILED(hr)) throw std::runtime_error("Depth target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));
}

inline DepthTarget::DepthTarget(ID3D11Device* device, UINT width, UINT height,
	DXGI_FORMAT textureFormat, DXGI_FORMAT depthFormat, DXGI_FORMAT shaderFormat)
	: width(width), height(height)
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = textureFormat;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	CheckCreation(device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf()));

	D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc = {};
	depthDesc.Format = depthFormat;
	depthDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	CheckCreation(device->CreateDepthStencilView(texture.Get(), &depthDesc, dsv.GetAddressOf()));
	depthDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
	if (depthFormat == DXGI_FORMAT_D24_UNORM_S8_UINT || depthFormat == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
		depthDesc.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
	CheckCreation(device->CreateDepthStencilView(texture.Get(), &depthDesc, readOnlyDsv.GetAddressOf()));

	D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc = {};
	shaderDesc.Format = shaderFormat;
	shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderDesc.Texture2D.MipLevels = 1;
	CheckCreation(device->CreateShaderResourceView(texture.Get(), &shaderDesc, srv.GetAddressOf()));
}

inline void DepthTarget::Clear(ID3D11DeviceContext* dc, UINT flags, float depth, UINT8 stencil) const
{
	dc->ClearDepthStencilView(dsv.Get(), flags, depth, stencil);
}
