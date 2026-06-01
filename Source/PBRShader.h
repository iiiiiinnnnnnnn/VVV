// PBRShader.h

#pragma once

#include "Shader.h"

class PBRShader : public ModelShader
{
public:
	PBRShader(ID3D11Device* device);
	~PBRShader() override = default;

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, const Model::Mesh& mesh) override;
	void End(const RenderContext& rc) override;

private:
	// b0
	struct CbShadowMap
	{
		Matrix	lightViewProjection;	// ライトビュープロジェクション行列
		Color	shadowColor;			// 影の色
		float	shadowBias;				// 深度比較用のオフセット値
		int		pcfKernelSize;			// ソフトシャドウの行列サイズ
		float	DUMMY[2];
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer>	shadowMapConstantBuffer;

	// b1
	struct CbMaterial
	{
		Color	baseColor;		// ベースカラー
		Color	emissiveColor;	// エミッシブカラー
		float	metalness;		// メタルネス
		float	roughness;		// ラフネス
		float	occlusionStrength;	// オクルージョン強度
		BOOL    hasMetalRoughTexture;	// ベースカラーテクスチャの有無
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer>	materialConstantBuffer;
};
