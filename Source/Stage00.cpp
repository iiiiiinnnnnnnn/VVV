// Stage00.cpp

#include "Stage00.h"
#include "ResourceManager.h"

Stage00::Stage00() : Actor("Stage00", "Stage00", "Default")
{
	std::shared_ptr<Model> model =
		ResourceManager::Instance().LoadModel("Data/Model/Stage00.glb");

	transform = Transform::FromScale(100);

	model->UpdateTransform(transform.matrix);

	// Rigidbody生成
	auto* rb = AddComponent<RigidbodyStatic>();

	// コライダー生成
	AddComponent<MeshCollider>(rb, model.get());

	// モデルレンダラー生成
	shaderParamWithMaterialName =
	{
		{
			"planem",
			{
				{"metalness", 0.0f},
				{"roughness", 1.0f},
				{"occlusionStrength", 1.0f}
			}
		},
		{
			"standardSurface1",
			{
				{"metalness", 1.0f},
				{"roughness", 0.1f},
				{"occlusionStrength", 1.0f}
			}
		}
	};
	AddComponent<ModelRenderComponent>(
		model, ModelShaderId::PBR, shaderParamWithMaterialName);
}

void Stage00::OnUpdate(float elapsedTime)
{

}

void Stage00::OnDrawGUI(float elapsedTime)
{

}
