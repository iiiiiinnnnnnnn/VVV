#include "RenderTarget.h"
#include "Misc.h"

// オフスクリーン用コンストラクタ
RenderTarget::RenderTarget(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format)
    : width(width), height(height)
{
    HRESULT hr;

    // カラーテクスチャ（RTV + SRV 兼用）
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
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = device->CreateRenderTargetView(tex.Get(), nullptr, rtv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    // 深度ステンシルテクスチャ
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        hr = device->CreateTexture2D(&desc, nullptr, tex.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = device->CreateDepthStencilView(tex.Get(), nullptr, dsv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    // ビューポート
    viewport.Width    = static_cast<float>(width);
    viewport.Height   = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
}

// バックバッファ用コンストラクタ
RenderTarget::RenderTarget(ID3D11Device* device, IDXGISwapChain* swapchain, UINT width, UINT height)
    : width(width), height(height)
{
    HRESULT hr;

    // スワップチェーンのバックバッファからRTVを生成（SRVは作らない）
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(backBuffer.GetAddressOf()));
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    // 深度ステンシルテクスチャ
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width            = width;
        desc.Height           = height;
        desc.MipLevels        = 1;
        desc.ArraySize        = 1;
        desc.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleDesc.Count = 1;
        desc.Usage            = D3D11_USAGE_DEFAULT;
        desc.BindFlags        = D3D11_BIND_DEPTH_STENCIL;
        hr = device->CreateTexture2D(&desc, nullptr, tex.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

        hr = device->CreateDepthStencilView(tex.Get(), nullptr, dsv.GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    // ビューポート
    viewport.Width    = static_cast<float>(width);
    viewport.Height   = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
}

void RenderTarget::Activate(ID3D11DeviceContext* dc)
{
    UINT numVp = 1;
    dc->RSGetViewports(&numVp, &prevViewport);
    dc->OMGetRenderTargets(1, prevRtv.ReleaseAndGetAddressOf(), prevDsv.ReleaseAndGetAddressOf());

    dc->RSSetViewports(1, &viewport);
    dc->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
}

void RenderTarget::Deactivate(ID3D11DeviceContext* dc)
{
    dc->RSSetViewports(1, &prevViewport);
    dc->OMSetRenderTargets(1, prevRtv.GetAddressOf(), prevDsv.Get());

    prevRtv.Reset();
    prevDsv.Reset();
}

void RenderTarget::Clear(ID3D11DeviceContext* dc, float r, float g, float b, float a)
{
    float color[4]{ r, g, b, a };
    dc->ClearRenderTargetView(rtv.Get(), color);
    dc->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}
