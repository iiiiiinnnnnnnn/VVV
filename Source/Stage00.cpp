// Stage00.cpp

#include "Stage00.h"

Stage00::Stage00() : Actor("Stage00", "Stage00", "Default")
{
	std::shared_ptr<Model> model = std::make_shared<Model>(
		"Data/Model/Stage/Stage00.glb");

	transform.scale = { 100, 100, 100 };
	transform.Update();
	model->UpdateTransform(transform.matrix);

	// Rigidbody生成
	auto* rb = AddComponent<RigidbodyStatic>();

	// コライダー生成
	AddComponent<MeshCollider>(rb, model.get());

	// モデルレンダラー生成
	AddComponent<ModelRender>(model);
}

void Stage00::OnUpdate(float elapsedTime)
{

}

void Stage00::OnRender(const RenderContext& rc, float elapsedTime)
{

}
