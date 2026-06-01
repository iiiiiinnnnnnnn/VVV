// Character.cpp

#include "Character.h"
#include "ResourceManager.h"
#include "GameTime.h"
#include "Graphics.h"

Character::Character(std::string name, std::string tag, bool isActive, std::string layer)
	: Actor(name, tag, isActive, layer)
{
	model = ResourceManager::Instance().LoadModel("Data/Model/CombatGirl_Shield/CombatGirls_Sword_Shield.glb");

	// キャラコン生成
	cc = AddComponent<CharacterController>(0.5f, 1.5f);

	// モデルレンダラー生成
	shaderParamWithMaterialName = {};
	AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMaterialName);

	// アニメーター生成
	anim = AddComponent<Animator>(model);
	anim->Load("Data/Animator/CombatGirls_Sword_Shield.animator");

	// ルートモーションボーンを設定
	anim->SetRootMotionBone(116);

	// テク変
	#if 0
	//HRESULT hr;

	//// Body
	//hr = Texture::LoadTexture(Game::Graphics::Instance().GetDevice(),
	//	"Data/Model/CombatGirl_Shield/Texture/Body/Body.png",
	//	&model->GetMaterials()[0].baseMap);
	//_ASSERT_EXPR(SUCCEEDED(hr), L"Failed to load texture: Body.png");

	//// Weapon_Axe_Shield
	//hr = Texture::LoadTexture(Game::Graphics::Instance().GetDevice(),
	//	"Data/Model/CombatGirl_Shield/Texture/Weapon/Weapon_Shield_Axe_01.png",
	//	&model->GetMaterials()[1].baseMap);
	//_ASSERT_EXPR(SUCCEEDED(hr), L"Failed to load texture: Weapon_Shield_Axe_01.png");

	//// Weapon_Sword_shield
	//hr = Texture::LoadTexture(Game::Graphics::Instance().GetDevice(),
	//	"Data/Model/CombatGirl_Shield/Texture/Weapon/Weapon_Shield_Sword_01.png",
	//	&model->GetMaterials()[2].baseMap);
	//_ASSERT_EXPR(SUCCEEDED(hr), L"Failed to load texture: Weapon_Shield_Sword_01.png");

	//// Face
	//hr = Texture::LoadTexture(Game::Graphics::Instance().GetDevice(),
	//	"Data/Model/CombatGirl_Shield/Texture/Body/Face.png",
	//	&model->GetMaterials()[3].baseMap);
	//_ASSERT_EXPR(SUCCEEDED(hr), L"Failed to load texture: Face.png");

	//// Eye
	//hr = Texture::LoadTexture(Game::Graphics::Instance().GetDevice(),
	//	"Data/Model/CombatGirl_Shield/Texture/Body/Eye.png",
	//	&model->GetMaterials()[4].baseMap);
	//_ASSERT_EXPR(SUCCEEDED(hr), L"Failed to load texture: Eye.png");

	//// Squire_Cloth
	//hr = Texture::LoadTexture(Game::Graphics::Instance().GetDevice(),
	//	"Data/Model/CombatGirl_Shield/Texture/Squire_Cloth/Squire_Cloth.png",
	//	&model->GetMaterials()[5].baseMap);
	//_ASSERT_EXPR(SUCCEEDED(hr), L"Failed to load texture: Squire_Cloth.png");

	//// Shield_Cloth
	//hr = Texture::LoadTexture(Game::Graphics::Instance().GetDevice(),
	//	"Data/Model/CombatGirl_Shield/Texture/Cloth/Shiled_Sword_Cloth_01.png",
	//	&model->GetMaterials()[6].baseMap);
	//_ASSERT_EXPR(SUCCEEDED(hr), L"Failed to load texture: Shiled_Sword_Cloth_01.png");

	//// Shield_Hair
	//hr = Texture::LoadTexture(Game::Graphics::Instance().GetDevice(),
	//	"Data/Model/CombatGirl_Shield/Texture/Hair/Hair_t.png",
	//	&model->GetMaterials()[7].baseMap);
	//_ASSERT_EXPR(SUCCEEDED(hr), L"Failed to load texture: Hair_t.png");
	#endif
}

void Character::OnUpdate()
{
	if (!controller) return;
	if (!cc) return;

	// 入力周り
	{
		float moveX = controller->GetMoveX();
		float moveZ = controller->GetMoveZ();

		// 移動ベクトル
		Vector3 move = Vector3::TransformNormal(
			Vector3(moveX, 0, moveZ),
			Matrix::CreateFromQuaternion(transform.rotation)
		);
		move *= speed * Game::Time::deltaTime;

		if (cc->IsGrounded())
			verticalVelocity = 0.0f;
		else
			verticalVelocity -= 9.81f * Game::Time::deltaTime;

		move.y = verticalVelocity * Game::Time::deltaTime;

		// アクターの向きを加味して、ルートモーションの移動量をワールド空間のベクトルに変換
		Vector3 rawDelta = anim->ConsumeRootMotionDelta();
		Vector3 worldDelta = Vector3::TransformNormal(rawDelta, transform.matrix);

		// 最終的にControllerで移動
		cc->Move(worldDelta + move);
	}
}

void Character::OnLateUpdate()
{

}

void Character::OnRender(const RenderContext& rc)
{

}

void Character::OnDrawGUI()
{

}
