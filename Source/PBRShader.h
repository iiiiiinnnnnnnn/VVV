#pragma once

#include "Shader.h"

class PBRShader : public ModelShader
{
public:
	PBRShader(ID3D11Device* device);
	~PBRShader() override = default;

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, const Model::Mesh& mesh, float elapsedTime) override;
	void ApplyParams(ShaderParamPtr params) override;
	void End(const RenderContext& rc) override;

	struct PBRData
	{
		float metalness;;
		float roughness;
	};

private:
	// シェーダー用
	struct CbPBR
	{
		Color materialColor;
		float adjustMetalness;
		float adjustRoughness;
		float DUMMY;
		float DUMMY;
	};

	// シャドウマップ用定数バッファ
	struct CbShadowMap
	{
		Matrix lightViewProjection;
		float shadowAttenuation;
		float shadowBias;
		float DUMMY;
		float DUMMY;
	};

	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> pbrBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> shadowMapBuffer;

	// 調整用
	PBRData pbrData;
};