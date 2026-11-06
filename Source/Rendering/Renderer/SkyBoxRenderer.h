// SkyBoxRenderer.h
#pragma once
#include <memory>
#include "Rendering/Core/ConstantBuffer.h"
#include "Rendering/Core/PixelShader.h"
#include "Rendering/Core/VertexShader.h"
#include "Rendering/Core/DepthStencilState.h"
#include "Rendering/Core/RasterizerState.h"
#include <d3d11.h>
#include <wrl.h>

#include "Core/Foundation/Common.h"
#include "Gameplay/Camera/Camera.h"
#include "Rendering/Core/BlendState.h"
namespace Game { class Graphics; }

struct RenderSettings;

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
                const Game::Graphics* graphicsResources,
                const Camera& camera,
                ID3D11ShaderResourceView* skyTex,
                const RenderSettings& renderSettings);

    void DrawGUI();

    float GetIntensity() const { return skyboxData.skyIntensity; }

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
		Color distanceFogColor;
		Vector4 distanceFogParams;
    };

    struct SkyboxData
    {
        float skyIntensity = 1.0f;
    } skyboxData;

    std::unique_ptr<Gpu::VertexShader> vertexShader;
    std::unique_ptr<Gpu::PixelShader> pixelShader;
    std::unique_ptr<Gpu::ConstantBuffer> constantBuffer;

    std::unique_ptr<Gpu::DepthStencilState> depthStencilState; // TestOnly (z=1)
    std::unique_ptr<Gpu::RasterizerState> rasterizerState;   // CullNone
};
