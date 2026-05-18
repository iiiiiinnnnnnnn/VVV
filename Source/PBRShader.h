#pragma once

#include "Shader.h"
#include "Graphics.h"

class PBRShader : public ModelShader
{
public:
	PBRShader(ID3D11Device* device);
	~PBRShader() override = default;

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, const Model::Mesh& mesh, float elapsedTime) override;
	void End(const RenderContext& rc) override;

private:
	struct CbMesh
	{
		Color		materialColor;
	};

	Microsoft::WRL::ComPtr<ID3D11Buffer>			meshConstantBuffer;
};
