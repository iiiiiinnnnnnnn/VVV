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

private:

	// シェーダー用
	struct CbPBR
	{
		Color		materialColor;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer>			constantBuffer;

	// 調整用
	struct PBRData {
		Color		materialColor;
	} pbrData;
};
