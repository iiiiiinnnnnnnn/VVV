// Player.cpp

#include "Player.h"
#include "ResourceManager.h"
#include "GameTime.h"
#include "Graphics.h"

Player::Player() : Actor("Player", "Player", true, "Default")
{
	model = ResourceManager::Instance().LoadModel("Data/Model/CombatGirl_Shield/CombatGirls_Sword_Shield.glb");
	model->GetMeshes()[0].isDraw = false; // 盾
	model->GetMeshes()[2].isDraw = false; // アックス
	model->GetMeshes()[8].isDraw = false; // 服
	model->GetMeshes()[15].isDraw = false;

	model->GetMeshes()[9].isDraw = false; // 素手

	model->GetMeshes()[4].isDraw = false; // 顔
	model->GetMeshes()[5].isDraw = false;
	model->GetMeshes()[16].isDraw = false;
	model->GetMeshes()[17].isDraw = false;
	model->GetMeshes()[18].isDraw = false;
	model->GetMeshes()[19].isDraw = false;
	model->GetMeshes()[20].isDraw = false;
	model->GetMeshes()[21].isDraw = false; 

	// モデルレンダラー生成
	shaderParamWithMaterialName = {};
	AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMaterialName);

	// アニメーター生成
	anim = AddComponent<Animator>(model);
	anim->Load("Data/Animator/CombatGirls_Sword_Shield.animator");

	// ルートモーションボーンを設定
	anim->SetRootMotionBone(116);

	// キャラコン生成
	cc = AddComponent<CharacterController>(0.5f, 1.5f);

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

void Player::OnUpdate()
{
	if (!controller || !cc) return;

	InputContext ctx = controller->Poll();

	// 入力をアニメーターに渡す
	anim->SetFloat("MoveX", ctx.moveX);
	anim->SetFloat("MoveZ", ctx.moveZ);
	anim->SetBool("Jump", ctx.jump);
	anim->SetBool("Ready", ctx.ready);
	anim->SetBool("Shoot", ctx.shoot);

	// 重力
	if (cc->IsGrounded())
		verticalVelocity = 0.0f;
	else
		verticalVelocity -= 9.81f * Game::Time::deltaTime;

	// 水平移動はrootMotionに任せる、垂直は重力のみ
	Vector3 rootDelta = anim->ConsumeRootMotionDelta();
	Vector3 worldDelta = Vector3::TransformNormal(rootDelta, transform.matrix);
	worldDelta.y = verticalVelocity * Game::Time::deltaTime;

	cc->Move(worldDelta);
}

void Player::OnLateUpdate()
{

}

void Player::OnRender(const RenderContext& rc)
{

}

void Player::OnDrawGUI()
{

}
