// RenderTarget.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include "Common.h"

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
    void Clear(ID3D11DeviceContext* dc, float r = 0, float g = 0, float b = 0, float a = 1);

    ID3D11ShaderResourceView* GetSRV() const { return srv.Get(); }
    ID3D11ShaderResourceView* GetDepthSRV() const { return depthSrv.Get(); }

    UINT GetWidth()  const { return width; }
    UINT GetHeight() const { return height; }

private:
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   dsv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthSrv;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   prevRtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   prevDsv;
    D3D11_VIEWPORT prevViewport = {};

    UINT width;
    UINT height;
    D3D11_VIEWPORT viewport = {};
};
