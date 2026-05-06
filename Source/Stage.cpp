// Stage.cpp

#include "Stage.h"
#include <Graphics.h>
#include <GpuResourceUtils.h>

Stage::Stage() : RenderActor("Data/Model/Stage/Stage00.glb")
{

}

void Stage::Update(float elapsedTime)
{
	// トランスフォーム更新
	transform.Update();
	model->UpdateTransform(Matrix::CreateScale(100, 100, 100));
}

void Stage::Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer)
{
	// モデル描画
	renderer->Draw(ShaderId::Lambert, model);
	renderer->Render(rc);
}
