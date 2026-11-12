#pragma once

#include <Dx11Kit/Common.h>

class RenderTarget
{
  public:
	RenderTarget(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format);
	RenderTarget(ID3D11Device* device, IDXGISwapChain* swapchain, UINT width, UINT height);

	void Clear(ID3D11DeviceContext* dc, float r = 0, float g = 0, float b = 0, float a = 1);
	ID3D11RenderTargetView* GetRTV() const { return rtv.Get(); }
	ID3D11ShaderResourceView* GetSRV() const { return srv.Get(); }
	UINT GetWidth() const { return width; }
	UINT GetHeight() const { return height; }

  private:
	static void CheckCreation(HRESULT hr);
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	UINT width;
	UINT height;
};

inline void RenderTarget::CheckCreation(HRESULT hr)
{
	if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));
}

inline RenderTarget::RenderTarget(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format)
	: width(width), height(height)
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	CheckCreation(device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf()));
	CheckCreation(device->CreateRenderTargetView(texture.Get(), nullptr, rtv.GetAddressOf()));
	CheckCreation(device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf()));
}

inline RenderTarget::RenderTarget(ID3D11Device* device, IDXGISwapChain* swapchain, UINT width, UINT height)
	: width(width), height(height)
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	CheckCreation(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(backBuffer.GetAddressOf())));
	CheckCreation(device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv.GetAddressOf()));
}

inline void RenderTarget::Clear(ID3D11DeviceContext* dc, float r, float g, float b, float a)
{
	const float color[]{r, g, b, a};
	dc->ClearRenderTargetView(rtv.Get(), color);
}
