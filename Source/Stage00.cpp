// Stage00.cpp

#include "Stage00.h"

Stage00::Stage00()
{
	std::shared_ptr<Model> model = std::make_shared<Model>(
		"Data/Model/Stage/Stage00.glb");

	AddComponent<ModelRender>(model);

	transform.scale = { 100, 100, 100 };
}

void Stage00::OnUpdate(float elapsedTime)
{

}

void Stage00::OnRender(const RenderContext& rc, float elapsedTime)
{

}
