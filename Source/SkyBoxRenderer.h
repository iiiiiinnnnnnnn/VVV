// SkyBoxRenderer.h
#pragma once
#include <d3d11.h>
#include <wrl.h>

#include "Common.h"
#include "Camera.h"
#include "RenderState.h"

// IBLキューブマップをスカイとして描画するレンダラー
// フルスクリーントライアングル1枚で描画する（頂点バッファ不要）
class SkyBoxRenderer
{
public:
    SkyBoxRenderer(ID3D11Device* device);
    ~SkyBoxRenderer() = default;

    // 描画
    // skyTex      : スカイに使うキューブマップ SRV（通常 specular_pmrem）
    // skyIntensity: 明るさスケール
    void Render(ID3D11DeviceContext* dc,
                const RenderState* renderState,
                const Camera& camera,
                ID3D11ShaderResourceView* skyTex);

    void DrawGUI();

    void SetIntensity(float intensity)
    {
        skyboxData.skyIntensity = intensity;
	}

private:
    struct CbSkyBox
    {
        Matrix  inverseViewProjection;
        Vector3 viewPos;
        float   skyIntensity;
    };

    struct SkyboxData
    {
        float skyIntensity = 1.0f;
    } skyboxData;

    Microsoft::WRL::ComPtr<ID3D11VertexShader>   vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>    pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer>         constantBuffer;

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState; // TestOnly (z=1)
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   rasterizerState;   // CullNone
};
