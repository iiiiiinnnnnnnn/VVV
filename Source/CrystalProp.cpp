// CrystalProp.cpp

#include "CrystalProp.h"

#include "Graphics.h"
#include "ModelRenderer.h"
#include "ResourceManager.h"

CrystalProp::CrystalProp(const StageLoader::CrystalData& crystalData)
	: Actor("CrystalProp", "CrystalProp", true)
{
	ApplyStageData(crystalData);
}

void CrystalProp::ApplyStageData(const StageLoader::CrystalData& crystalData)
{
	parentTransform = crystalData.parentTransform;

	if (modelPath != crystalData.modelPath)
	{
		modelPath = crystalData.modelPath;
		instances.clear();
	}

	instances.resize(crystalData.transforms.size());

	for (size_t i = 0; i < crystalData.transforms.size(); ++i)
	{
		Instance& instance = instances[i];
		instance.transform = crystalData.transforms[i];

		if (!instance.model)
		{
			instance.model = ResourceManager::Instance().LoadModel(modelPath);
		}

		ModelRenderer::SetShaderParamForAllMaterials(
			instance.model.get(),
			crystalData.MakePBRParams(),
			shaderParams);
	}
}

void CrystalProp::Update()
{
	Actor::Update();
	parentTransform.Update();

	for (Instance& instance : instances)
	{
		instance.transform.Update();
		if (instance.model) instance.model->UpdateTransform(instance.transform.matrix * parentTransform.matrix);
	}
}

void CrystalProp::Render(const RenderContext& rc)
{
	for (const Instance& instance : instances)
	{
		if (!instance.model) continue;
		Game::Graphics::Instance().GetModelRenderer()->Draw(ModelShaderId::PBR, instance.model, shaderParams);
	}
}
