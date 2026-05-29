#pragma once

#include "Shader.h"

class BasicModelShader : public ModelShader
{
public:
	BasicModelShader(ID3D11Device* device);
	~BasicModelShader() override = default;

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, const Model::Mesh& mesh, float elapsedTime) override;
	void End(const RenderContext& rc) override;

private:
	struct CbBasic
	{
		Color		materialColor;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
};
