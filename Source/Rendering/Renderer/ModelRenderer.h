#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Resource/Model.h"
#include "Rendering/Shader/Shader.h"
#include "Gameplay/Lighting/CbLightData.h"

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

	static void SetShaderParamForAllMaterials(Model* model, const ShaderParam& param, ShaderParamListWithMaterialName& paramsWithMaterial);
	static void SetShaderParamForAllMaterials(Model* model, const ShaderParamList& paramList, ShaderParamListWithMaterialName& paramsWithMaterial);

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
