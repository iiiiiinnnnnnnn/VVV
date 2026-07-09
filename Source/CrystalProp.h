// CrystalProp.h

#pragma once

#include <memory>
#include <vector>

#include "Actor.h"
#include "Model.h"
#include "ShaderParam.h"
#include "StageLoader.h"

class CrystalProp : public Actor
{
public:
	CrystalProp(const StageLoader::CrystalData& crystalData);
	~CrystalProp() override = default;

	void ApplyStageData(const StageLoader::CrystalData& crystalData);
	void Update() override;
	void Render(const RenderContext& rc) override;

private:
	struct Instance
	{
		Transform transform = {};
		std::shared_ptr<Model> model = nullptr;
	};

	std::vector<Instance> instances = {};
	ShaderParamListWithMaterialName shaderParams = {};
	std::string modelPath = "";
	Transform parentTransform = {};
};
