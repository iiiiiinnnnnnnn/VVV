#pragma once
#include <memory>
#include <Dx11Kit/Dx11Kit.h>
#include <d3d11.h>
#include <wrl.h>

#include <array>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Shader/Shader.h"

class Terrain;

class ShadowMapRenderer
{
public:
    static constexpr int CascadeCount = ShadowMapData::CascadeCount;

    ShadowMapRenderer(ID3D11Device* device, UINT shadowMapSize = 1024 * 4);
    ~ShadowMapRenderer() = default;

    void Draw(VMDLModel* model);
    void Draw(VMDLModel* skeleton, const std::vector<VMDLModel::Mesh>* meshes);
    void Draw(Terrain* terrain);

    void Render(const RenderContext& rc,
                const Vector3& lightDir,
                float shadowDistance = 500.0f);

	void DrawGUI();

    ID3D11ShaderResourceView* GetDepthSRV(int cascadeIndex) const
    {
        return depthSrvs.at(cascadeIndex).Get();
    }
    const Matrix& GetLightViewProjection(int cascadeIndex) const
    {
        return lightViewProjections.at(cascadeIndex);
    }
    const Vector4& GetCascadeSplits() const { return cascadeSplits; }
    float GetShadowBias() const { return comparisonBias; }

private:
    void CreateRasterizerStates(ID3D11Device* device);
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
    std::unique_ptr<Gpu::ConstantBuffer> skeletonConstantBuffer;

    struct CbShadowScene
    {
        Matrix lightViewProjection;
    };
    std::unique_ptr<Gpu::ConstantBuffer> shadowSceneConstantBuffer;

    std::unique_ptr<Gpu::VertexShader> vertexShader;

    std::unique_ptr<Gpu::RasterizerState> rasterizerState;
    std::unique_ptr<Gpu::RasterizerState> terrainRasterizerState;
    std::unique_ptr<Gpu::DepthStencilState> depthStencilState;

	struct ModelDrawInfo
	{
		VMDLModel* skeleton = nullptr;
		const std::vector<VMDLModel::Mesh>* meshes = nullptr;
	};
	std::vector<ModelDrawInfo> drawList;
    std::vector<Terrain*> terrainDrawList;

    std::array<Matrix, CascadeCount> lightViewProjections;
    Vector4 cascadeSplits = {};
    UINT shadowMapSize;

	int depthBias = 1000;
	float slopeScaledDepthBias = 0.0001f;
	float comparisonBias = 0.0001f;
};
