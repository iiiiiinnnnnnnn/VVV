#pragma once
#include "Rendering/Shader/Shader.h"

class FunkyShader : public ModelShader
{
public:
	FunkyShader(ID3D11Device* device);
	~FunkyShader() override = default;

	ModelShaderId GetId() const override { return ModelShaderId::Funky; }
	const char* GetName() const override { return "Funky"; }
	bool DrawGUI(VMDLModel::Material& material) override;
	void Begin(const RenderContext& rc, DrawPass pass) override;
	void Update(const RenderContext& rc, const VMDLModel::Mesh& mesh,
		const PBRRenderParams* params, const PBRMaterialParams* materialParams) override;
	void End(const RenderContext& rc) override;

private:
	struct CbRainbowData
	{
		float lineWidth;
		float speed;
		float padding[2];
	};
	std::unique_ptr<Gpu::ConstantBuffer> rainbowConstantBuffer;
	std::unique_ptr<Gpu::ConstantBuffer> materialConstantBuffer;
};
