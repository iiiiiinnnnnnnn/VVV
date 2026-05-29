// Stage00.cpp

#include "Stage00.h"
#include "ResourceManager.h"

Stage00::Stage00() : Actor("Stage00", "Stage00", "Default")
{
	std::shared_ptr<Model> model =
		ResourceManager::Instance().LoadModel("Data/Model/Stage/Stage00.glb");

	transform = Transform::FromScale(100);

	model->UpdateTransform(transform.matrix);

	// Rigidbody生成
	auto* rb = AddComponent<RigidbodyStatic>();

	// コライダー生成
	AddComponent<MeshCollider>(rb, model.get());

	// モデルレンダラー生成
	AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, &shaderParam);
}

void Stage00::OnUpdate(float elapsedTime)
{

}

void Stage00::OnRender(const RenderContext& rc, float elapsedTime)
{

}

void Stage00::OnDrawGUI(float elapsedTime)
{
	ImGui::DragFloat("Metalness", &shaderParam.metalness, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Roughness", &shaderParam.roughness, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Occlusion Strength", &shaderParam.occlusionStrength, 0.01f, 0.0f, 5.0f);
}
