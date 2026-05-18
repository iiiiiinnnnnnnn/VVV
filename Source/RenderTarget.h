#pragma once

#include "Common.h"

class RenderTarget
{
public:
    RenderTarget(ID3D11Device* device, UINT width, UINT height,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
    ~RenderTarget() = default;

    // ï`âÊêÊêÿÇËë÷Ç¶
    void Activate(ID3D11DeviceContext* dc);

    // å≥ÇÃRTÇ…ñﬂÇ∑
    void Deactivate(ID3D11DeviceContext* dc);

    // ÉNÉäÉA
    void Clear(ID3D11DeviceContext* dc, float r = 0, float g = 0, float b = 0, float a = 1);

    ID3D11ShaderResourceView* GetSRV() const { return srv.Get(); }
    UINT GetWidth()  const { return width; }
    UINT GetHeight() const { return height; }

private:
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   dsv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

    // å≥ÇÃRTÇ…ñﬂÇ∑ÇΩÇﬂÇ…ï€ë∂
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   prevRtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   prevDsv;
    D3D11_VIEWPORT prevViewport = {};

    UINT width;
    UINT height;
    D3D11_VIEWPORT viewport = {};
};
