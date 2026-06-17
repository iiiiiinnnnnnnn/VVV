#pragma once

#include "Common.h"
#include "Model.h"
#include "Shader.h"
#include "CbLightData.h"

enum class ModelShaderId
{
	Basic,
	PBR,

	EnumCount
};

class ModelRenderer
{
public:
	ModelRenderer(ID3D11Device* device);
	~ModelRenderer() {}

	void Draw(ModelShaderId shaderId, std::shared_ptr<Model> model, std::unordered_map<std::string, ShaderParamList> paramsWithMaterial);

	void Render(const RenderContext& rc);

private:
	
	struct CbScene
	{
		Matrix		viewProjection;
		Vector3		viewPosition;
		float DUMMY;
		CbLightData lightData;
	};

	struct CbSkeleton
	{
		Matrix		boneTransforms[256];
	};

	struct DrawInfo
	{
		ModelShaderId				shaderId;
		std::shared_ptr<Model>	model;
		std::unordered_map<std::string, ShaderParamList> paramsWithMaterial;
	};

	struct TransparencyDrawInfo
	{
		ModelShaderId				shaderId;
		const Model::Mesh*		mesh;
		float					distance;
		std::unordered_map<std::string, ShaderParamList> paramsWithMaterial;
	};

	std::unique_ptr<ModelShader>			shaders[static_cast<int>(ModelShaderId::EnumCount)];
	std::vector<DrawInfo>					drawInfos;
	std::vector<TransparencyDrawInfo>		transparencyDrawInfos;

	Microsoft::WRL::ComPtr<ID3D11Buffer>	sceneConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>	skeletonConstantBuffer;
};
