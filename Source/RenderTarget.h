#pragma once
#include <d3d11.h>
#include <wrl.h>

#include "Common.h"

class RenderTarget
{
public:
    // オフスクリーン用（テクスチャを自前生成）
    RenderTarget(ID3D11Device* device, UINT width, UINT height,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

    // バックバッファ用（スワップチェーンのRTVを受け取る）
    RenderTarget(ID3D11Device* device, IDXGISwapChain* swapchain, UINT width, UINT height);

    ~RenderTarget() = default;

    // 描画先切り替え
    void Activate(ID3D11DeviceContext* dc);

    // 元のRTに戻す
    void Deactivate(ID3D11DeviceContext* dc);

    // クリア
    void Clear(ID3D11DeviceContext* dc, float r = 0, float g = 0, float b = 0, float a = 1);

    // オフスクリーン用のみ有効
    ID3D11ShaderResourceView* GetSRV() const { return srv.Get(); }

    UINT GetWidth()  const { return width; }
    UINT GetHeight() const { return height; }

private:
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   dsv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv; // バックバッファ用はnull

    // 元のRTに戻すために保存
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   prevRtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   prevDsv;
    D3D11_VIEWPORT prevViewport = {};

    UINT width;
    UINT height;
    D3D11_VIEWPORT viewport = {};
};
