// ShadowMapRenderer.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <vector>

#include "Common.h"
#include "Model.h"
#include "Shader.h"

class Terrain;

class ShadowMapRenderer
{
public:
    ShadowMapRenderer(ID3D11Device* device, UINT shadowMapSize = 2048 * 4);
    ~ShadowMapRenderer() = default;

    void Draw(Model* model);
    void Draw(Terrain* terrain);

    void Render(const RenderContext& rc,
                const Vector3& lightDir,
                const Vector3& targetPos,
                float lightDistance = 50.0f,
                float orthoSize = 30.0f,
                float nearZ = 0.1f,
                float farZ = 200.0f);

    ID3D11ShaderResourceView* GetDepthSRV() const { return depthSrv.Get(); }
    const Matrix& GetLightViewProjection() const { return lightViewProjection; }

private:
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthDsv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthSrv;
    D3D11_VIEWPORT shadowViewport = {};

    struct CbSkeleton
    {
        Matrix boneTransforms[256];
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> skeletonConstantBuffer;

    struct CbShadowScene
    {
        Matrix lightViewProjection;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> shadowSceneConstantBuffer;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> terrainRasterizerState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;

    std::vector<Model*> drawList;
    std::vector<Terrain*> terrainDrawList;

    Matrix lightViewProjection;
    UINT shadowMapSize;
};
