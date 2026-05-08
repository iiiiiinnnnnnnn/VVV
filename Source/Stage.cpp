// Stage.cpp

#include "Stage.h"
#include "Components.h"

Stage::Stage()
{
	std::shared_ptr<Model> model = std::make_shared<Model>(
		"Data/Model/Stage/Stage00.glb");

	AddComponent<ModelRender>(model);

	transform.scale = { 100, 100, 100 };
}

void Stage::OnUpdate(float elapsedTime)
{

}

void Stage::OnRender(const RenderContext& rc, float elapsedTime)
{

}
