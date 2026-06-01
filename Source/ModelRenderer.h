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

	void Render(const RenderContext& rc);

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
	};
	struct CbAreaLight
	{
		Vector3 position = {0, 0, 0};
		float   width = 1.0f;
		Vector3 direction = {0, -1, 0};  // 法線方向
		float   height = 1.0f;
		Vector3 right = {1, 0, 0};   // 矩形のX軸
		float   range = 10.0f;
		Color   color = {1, 1, 1, 1};
	};
	struct CbLightManager
	{
		CbDirectionalLight directionalLight;
		CbPointLight pointLights[LightData::MaxPointLights];
		CbSpotLight spotLights[LightData::MaxSpotLights];
		CbAreaLight areaLights[LightData::MaxAreaLights];
		Color ambientColor;
		int pointLightCount;
		int spotLightCount;
		int areaLightCount;
		float DUMMY;
		CbLightManager() {}
		CbLightManager(const LightData& lm)
		{
			directionalLight.direction = lm.GetDirectionalLight().direction;
			directionalLight.color = lm.GetDirectionalLight().color;
			pointLightCount = static_cast<int>(lm.GetPointLights().size());
			for (int i = 0; i < pointLightCount; i++)
			{
				pointLights[i].position = lm.GetPointLights()[i].position;
				pointLights[i].range = lm.GetPointLights()[i].range;
				pointLights[i].color = lm.GetPointLights()[i].color;
			}
			spotLightCount = static_cast<int>(lm.GetSpotLights().size());
			for (int i = 0; i < spotLightCount; i++)
			{
				spotLights[i].position = lm.GetSpotLights()[i].position;
				spotLights[i].direction = lm.GetSpotLights()[i].direction;
				spotLights[i].range = lm.GetSpotLights()[i].range;
				spotLights[i].innerConeAngle = lm.GetSpotLights()[i].innerConeAngle;
				spotLights[i].outerConeAngle = lm.GetSpotLights()[i].outerConeAngle;
				spotLights[i].color = lm.GetSpotLights()[i].color;
			}
			areaLightCount = static_cast<int>(lm.GetAreaLights().size());
			for (int i = 0; i < areaLightCount; i++)
			{
				areaLights[i].position = lm.GetAreaLights()[i].position;
				areaLights[i].direction = lm.GetAreaLights()[i].direction;
				areaLights[i].right = lm.GetAreaLights()[i].right;
				areaLights[i].width = lm.GetAreaLights()[i].width;
				areaLights[i].height = lm.GetAreaLights()[i].height;
				areaLights[i].range = lm.GetAreaLights()[i].range;
				areaLights[i].color = lm.GetAreaLights()[i].color;
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
