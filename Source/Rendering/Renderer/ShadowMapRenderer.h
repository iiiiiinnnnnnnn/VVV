// ShadowMapRenderer.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <array>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Resource/Model.h"
#include "Rendering/Shader/Shader.h"

class Terrain;

class ShadowMapRenderer
{
public:
    static constexpr int CascadeCount = ShadowMapData::CascadeCount;

    ShadowMapRenderer(ID3D11Device* device, UINT shadowMapSize = 2048);
    ~ShadowMapRenderer() = default;

    void Draw(Model* model);
    void Draw(Terrain* terrain);

    void Render(const RenderContext& rc,
                const Vector3& lightDir,
                float shadowDistance = 500.0f);

    ID3D11ShaderResourceView* GetDepthSRV(int cascadeIndex) const
    {
        return depthSrvs.at(cascadeIndex).Get();
    }
    const Matrix& GetLightViewProjection(int cascadeIndex) const
    {
        return lightViewProjections.at(cascadeIndex);
    }
    const Vector4& GetCascadeSplits() const { return cascadeSplits; }

private:
    void CalculateCascadeMatrices(
        const Camera& camera,
        const Vector3& lightDir,
        float shadowDistance);
    void RenderCascade(ID3D11DeviceContext* dc, int cascadeIndex);

    std::array<Microsoft::WRL::ComPtr<ID3D11DepthStencilView>, CascadeCount> depthDsvs;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, CascadeCount> depthSrvs;
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

    std::array<Matrix, CascadeCount> lightViewProjections;
    Vector4 cascadeSplits = {};
    UINT shadowMapSize;
};
