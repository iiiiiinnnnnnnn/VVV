#pragma once

#include "Common.h"
#include "Model.h"
#include "Shader.h"

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

	void Render(const RenderContext& rc, float elapsedTime);

private:
	struct CbDirectionalLight
	{
		Vector3		direction;
		float DUMMY;
		Color		color;
	};
	struct CbPointLight
	{
		Vector3		position;
		float		range;
		Color		color;
	};
	struct CbSpotLight
	{
		Vector3	position;
		float DUMMY;
		Vector3	direction;
		float DUMMY;
		Color	color;
		float	range;
		float	innerConeAngle;
		float	outerConeAngle;
		float DUMMY;
	};;
	struct CbLightManager {
		CbDirectionalLight directionalLight;
		CbPointLight pointLights[LightData::MaxPointLights];
		CbSpotLight spotLights[LightData::MaxSpotLights];
		Color ambientColor;
		unsigned int pointLightCount;
		unsigned int spotLightCount;
		float DUMMY;
		float DUMMY;
		CbLightManager() {}
		CbLightManager(const LightData& lm) {
			directionalLight.direction = lm.GetDirectionalLight().direction;
			directionalLight.color = lm.GetDirectionalLight().color;
			for (int i = 0; i < LightData::MaxPointLights; i++) {
				pointLights[i].position = lm.GetPointLights()[i].position;
				pointLights[i].range = lm.GetPointLights()[i].range;
				pointLights[i].color = lm.GetPointLights()[i].color;
			}
			for (int i = 0; i < LightData::MaxSpotLights; i++) {
				spotLights[i].position = lm.GetSpotLights()[i].position;
				spotLights[i].direction = lm.GetSpotLights()[i].direction;
				spotLights[i].range = lm.GetSpotLights()[i].range;
				spotLights[i].innerConeAngle = lm.GetSpotLights()[i].innerConeAngle;
				spotLights[i].outerConeAngle = lm.GetSpotLights()[i].outerConeAngle;
				spotLights[i].color = lm.GetSpotLights()[i].color;
			}
			ambientColor = lm.GetAmbientColor();
		}
	};
	struct CbScene
	{
		Matrix		viewProjection;
		Vector3		viewPosition;
		float DUMMY;
		CbLightManager lightManager;
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
