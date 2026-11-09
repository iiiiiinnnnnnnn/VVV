#pragma once

#include <Dx11Kit/Common.h>


class RenderTarget
{
  public:
	// Offscreen render target.
	RenderTarget(ID3D11Device* device, UINT width, UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

	// Swap chain back buffer render target.
	RenderTarget(ID3D11Device* device, IDXGISwapChain* swapchain, UINT width, UINT height);

	~RenderTarget() = default;

	void Activate(ID3D11DeviceContext* dc);
	void Deactivate(ID3D11DeviceContext* dc);
	void SetDepthReadOnly(ID3D11DeviceContext* dc, bool readOnly);
	void Clear(ID3D11DeviceContext* dc, float r = 0, float g = 0, float b = 0, float a = 1);

	ID3D11ShaderResourceView* GetSRV() const
	{
		return srv.Get();
	}
	ID3D11ShaderResourceView* GetDepthSRV() const
	{
		return depthSrv.Get();
	}

	UINT GetWidth() const
	{
		return width;
	}
	UINT GetHeight() const
	{
		return height;
	}

	struct OutputState
	{
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> color;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
		std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
		UINT viewportCount = 0;
	};
	static Microsoft::WRL::ComPtr<ID3D11RenderTargetView> GetColorTarget(ID3D11DeviceContext* context);
	static D3D11_VIEWPORT GetViewport(ID3D11DeviceContext* context);
	static void SetViewport(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport);
	static void BindDepthTarget(ID3D11DeviceContext* context, ID3D11DepthStencilView* depth);
	static void ClearDepth(ID3D11DeviceContext* context, ID3D11DepthStencilView* depth,
		float value = 1.0f, UINT8 stencil = 0, UINT flags = D3D11_CLEAR_DEPTH);
	static OutputState CaptureOutput(ID3D11DeviceContext* context);
	static void RestoreOutput(ID3D11DeviceContext* context, const OutputState& state);

  private:
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> readOnlyDsv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthSrv;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> prevRtv;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> prevDsv;
	D3D11_VIEWPORT prevViewport = {};

	UINT width;
	UINT height;
	D3D11_VIEWPORT viewport = {};
};

inline RenderTarget::RenderTarget(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format)
    : width(width), height(height)
{
    HRESULT hr;

    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        hr = device->CreateTexture2D(&desc, nullptr, tex.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));

        hr = device->CreateRenderTargetView(tex.Get(), nullptr, rtv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));

        hr = device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));
    }

    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        hr = device->CreateTexture2D(&desc, nullptr, tex.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = device->CreateDepthStencilView(tex.Get(), &dsvDesc, dsv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));

		dsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL;
		hr = device->CreateDepthStencilView(tex.Get(), &dsvDesc, readOnlyDsv.GetAddressOf());
		if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        hr = device->CreateShaderResourceView(tex.Get(), &srvDesc, depthSrv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));
    }

    viewport.Width    = static_cast<float>(width);
    viewport.Height   = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
}

inline RenderTarget::RenderTarget(ID3D11Device* device, IDXGISwapChain* swapchain, UINT width, UINT height)
    : width(width), height(height)
{
    HRESULT hr;

    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(backBuffer.GetAddressOf()));
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));

        hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));
    }

    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width            = width;
        desc.Height           = height;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_R24G8_TYPELESS;
        desc.SampleDesc.Count = 1;
        desc.Usage            = D3D11_USAGE_DEFAULT;
        desc.BindFlags        = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        hr = device->CreateTexture2D(&desc, nullptr, tex.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = device->CreateDepthStencilView(tex.Get(), &dsvDesc, dsv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));

		dsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL;
		hr = device->CreateDepthStencilView(tex.Get(), &dsvDesc, readOnlyDsv.GetAddressOf());
		if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        hr = device->CreateShaderResourceView(tex.Get(), &srvDesc, depthSrv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Render target creation failed. HRESULT: " + std::to_string(static_cast<unsigned long>(hr)));
    }

    viewport.Width    = static_cast<float>(width);
    viewport.Height   = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
}

inline void RenderTarget::Activate(ID3D11DeviceContext* dc)
{
    UINT numVp = 1;
    dc->RSGetViewports(&numVp, &prevViewport);
    dc->OMGetRenderTargets(1, prevRtv.ReleaseAndGetAddressOf(), prevDsv.ReleaseAndGetAddressOf());

    dc->RSSetViewports(1, &viewport);
    dc->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
}

inline void RenderTarget::Deactivate(ID3D11DeviceContext* dc)
{
    dc->RSSetViewports(1, &prevViewport);
    dc->OMSetRenderTargets(1, prevRtv.GetAddressOf(), prevDsv.Get());

    prevRtv.Reset();
    prevDsv.Reset();
}

inline void RenderTarget::SetDepthReadOnly(ID3D11DeviceContext* dc, bool readOnly)
{
	dc->OMSetRenderTargets(1, rtv.GetAddressOf(), readOnly ? readOnlyDsv.Get() : dsv.Get());
}

inline void RenderTarget::Clear(ID3D11DeviceContext* dc, float r, float g, float b, float a)
{
	float color[4]{r, g, b, a};
	dc->ClearRenderTargetView(rtv.Get(), color);
	dc->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

inline Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderTarget::GetColorTarget(ID3D11DeviceContext* context)
{
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
	context->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
	return target;
}

inline D3D11_VIEWPORT RenderTarget::GetViewport(ID3D11DeviceContext* context)
{
	D3D11_VIEWPORT viewport{};
	UINT count = 1;
	context->RSGetViewports(&count, &viewport);
	return viewport;
}

inline void RenderTarget::SetViewport(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport)
{
	context->RSSetViewports(1, &viewport);
}

inline void RenderTarget::BindDepthTarget(ID3D11DeviceContext* context, ID3D11DepthStencilView* depth)
{
	ID3D11RenderTargetView* color = nullptr;
	context->OMSetRenderTargets(1, &color, depth);
}

inline void RenderTarget::ClearDepth(ID3D11DeviceContext* context, ID3D11DepthStencilView* depth, float value, UINT8 stencil, UINT flags)
{
	context->ClearDepthStencilView(depth, flags, value, stencil);
}

inline RenderTarget::OutputState RenderTarget::CaptureOutput(ID3D11DeviceContext* context)
{
	OutputState state;
	context->OMGetRenderTargets(1, state.color.GetAddressOf(), state.depth.GetAddressOf());
	state.viewportCount = static_cast<UINT>(state.viewports.size());
	context->RSGetViewports(&state.viewportCount, state.viewports.data());
	return state;
}

inline void RenderTarget::RestoreOutput(ID3D11DeviceContext* context, const OutputState& state)
{
	context->OMSetRenderTargets(1, state.color.GetAddressOf(), state.depth.Get());
	context->RSSetViewports(state.viewportCount, state.viewports.data());
}
