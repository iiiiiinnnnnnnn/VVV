// Player.cpp

#include "Player.h"
#include "ResourceManager.h"
#include "GameTime.h"
#include "Graphics.h"

Player::Player() : Actor("Player", "Player", true, "Default")
{
	model = ResourceManager::Instance().LoadModel("Data/Model/CombatGirl_Shield/CombatGirls_Sword_Shield.glb");
	// メッシュ表示/非表示
	{
		auto& meshes = model->GetMeshes();
		// 盾
		meshes[0].isDraw = false;
		// アックス
		meshes[2].isDraw = false;
		// 服
		meshes[8].isDraw =
			meshes[15].isDraw = false;
		// 素手
		meshes[9].isDraw = false;
		// 顔
		meshes[4].isDraw =
			meshes[5].isDraw =
			meshes[16].isDraw =
			meshes[17].isDraw =
			meshes[18].isDraw =
			meshes[19].isDraw =
			meshes[20].isDraw =
			meshes[21].isDraw = false;
	}

	// モデルレンダラー生成
	shaderParamWithMaterialName = {};
	AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMaterialName);

	// アニメーター生成
	anim = AddComponent<Animator>(model);
	anim->SetRootMotion("root");
	anim->Load("Data/Animator/CombatGirls_Sword_Shield.animator");

	// キャラクターコントローラ生成
	cc = AddComponent<CharacterController>(0.18f, 1.18f);
	cc->SetPosition({0, 0.6f, 0});
}

void Player::OnUpdate()
{
	if (!controller || !cc) return;

	InputContext ctx = controller->Poll();

	// Speed を計算（入力ベクトルの長さ）
	float inputLen = sqrtf(ctx.moveX * ctx.moveX + ctx.moveZ * ctx.moveZ);
	// Sprint中は1.5、通常は最大1.0にスケール
	bool sprinting = ctx.sprint && inputLen > 0.1f;
	float speed = sprinting ? 1.5f : inputLen;      // 0.0〜1.0 or 1.5

	// Animatorに渡す
	anim->SetFloat("Speed", speed);
	anim->SetBool("IsSprinting", sprinting);

	// 重力（既存のまま）
	if (cc->IsGrounded())
		verticalVelocity = 0.0f;
	else
		verticalVelocity -= 9.81f * Game::Time::deltaTime;

	cc->Move({0, verticalVelocity * Game::Time::deltaTime, 0});
}

void Player::OnLateUpdate()
{
	// 1. Animatorから最新フレームのルートモーション（ローカル差分）を回収
	Vector3 localMoveVec = anim->GetRootMotionVec();
	Quaternion deltaRot  = anim->GetRootMotionRot();

	// 2. 回転の適用（現在の向きにアニメーションの回転差分を乗算）
	Quaternion currentRot = transform.rotation;
	transform.SetRotation(currentRot * deltaRot);

	// 3. ローカルの移動量を、プレイヤーの最新の向きに合わせて「ワールド空間」に変換
	Vector3 worldMoveVec = Vector3::Transform(localMoveVec, transform.rotation);

	// 4. 移動の適用
	Vector3 currentPos = transform.position;
	cc->Move(worldMoveVec);
}

void Player::OnRender(const RenderContext& rc)
{

}

void Player::OnDrawGUI()
{

}