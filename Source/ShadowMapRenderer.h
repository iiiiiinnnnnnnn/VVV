// ShadowMapRenderer.h

#pragma once

#include "Common.h"
#include "Model.h"
#include "Shader.h"

// シャドウマップ描画クラス
// 役割: ライト視点でシーン内メッシュのデプスをテクスチャに書き出す
// PBRShaderなどは RenderContext::shadowMapData 経由でこの結果を参照する
class ShadowMapRenderer
{
public:
    ShadowMapRenderer(ID3D11Device* device, UINT shadowMapSize = 2048 * 4);
    ~ShadowMapRenderer() = default;

    // 描画登録
    void Draw(Model* model);

    // シャドウマップ生成パスの実行
    // lightDir: ライトの向き（正規化済み）
    // targetPos: ライトが注視する中心点（通常はプレイヤー/シーン中心）
    // lightDistance: ライト位置をtargetPosからどれだけ引いたか（遠すぎると精度低下）
    // orthoSize: 正射影の幅/高さ（シーンをカバーするサイズに合わせる）
    void Render(const RenderContext& rc,
                const Vector3& lightDir,
                const Vector3& targetPos,
                float lightDistance = 50.0f,
                float orthoSize = 30.0f,
                float nearZ = 0.1f,
                float farZ = 200.0f);

    // シャドウマップSRV（RenderContextに詰めてPBRShaderに渡す）
    ID3D11ShaderResourceView* GetDepthSRV() const { return depthSrv.Get(); }

    // ライトのViewProjection行列（RenderContextに詰める）
    const Matrix& GetLightViewProjection() const { return lightViewProjection; }

private:
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   depthDsv;
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
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState>    rasterizerState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  depthStencilState;

    std::vector<Model*> drawList;

    Matrix lightViewProjection;

    UINT shadowMapSize;
};
