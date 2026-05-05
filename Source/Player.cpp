// Player.cpp

#include "Player.h"
#include <Graphics.h>
#include <GpuResourceUtils.h>

Player::Player()
{
	auto device = Graphics::Instance().GetDevice();

	// モデル生成
	model = std::make_shared<Model>(device, "Data/Model/ToonSoldiers_WW2/models/ToonSoldier_WW2_Japan_Soldier.glb");

	// materialにセット
	GpuResourceUtils::LoadTexture(
		Graphics::Instance().GetDevice(),
		"Data/Model/ToonSoldiers_WW2/models/textures/TS_WW2_Japan_Infantry.tga",
		model->GetMaterials()[0].baseMap.GetAddressOf()
	);
	GpuResourceUtils::LoadTexture(
		Graphics::Instance().GetDevice(),
		"Data/Model/ToonSoldiers_WW2/models/textures/TS_WW2_weapons.tga",
		model->GetMaterials()[1].baseMap.GetAddressOf()
	);

	model->GetMeshes()[2].isDraw = false;
	model->GetMeshes()[4].isDraw = false;
	model->GetMeshes()[5].isDraw = false;
	model->GetMeshes()[6].isDraw = false;
	model->GetMeshes()[7].isDraw = false;
	model->GetMeshes()[8].isDraw = false;
	model->GetMeshes()[9].isDraw = false;
	model->GetMeshes()[10].isDraw = false; // ガスマスク
	model->GetMeshes()[11].isDraw = false; // ガスマスク

	// アニメーション追加
	model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_walk.glb");

	// アニメーター生成
	animator = std::make_shared<Animator>(model.get());
	animator->Play(0, true);
}

void Player::Update(float elapsedTime)
{
	if (animator)
		animator->Update(elapsedTime);

	model->UpdateTransform(Matrix::CreateScale(1, 1, 1));
}

void Player::Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer)
{
	// モデル描画
	renderer->Draw(ShaderId::Lambert, model);
	renderer->Render(rc);
}
