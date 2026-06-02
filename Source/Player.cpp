// Player.cpp

#include "Player.h"
#include "ResourceManager.h"
#include "GameTime.h"

Player::Player() : Actor("Player", "Player", true, "Default")
{
	model = ResourceManager::Instance().LoadModel("Data/Model/CombatGirl_Shield/CombatGirls_Sword_Shield.glb");

	// メッシュ表示/非表示
	{
		auto& meshes = model->GetMeshes();
		meshes[0].isDraw  = false; // 盾
		meshes[2].isDraw  = false; // アックス
		meshes[8].isDraw  =
			meshes[15].isDraw = false; // 服
		meshes[9].isDraw  = false; // 素手
		meshes[4].isDraw  =
			meshes[5].isDraw  =
			meshes[16].isDraw =
			meshes[17].isDraw =
			meshes[18].isDraw =
			meshes[19].isDraw =
			meshes[20].isDraw =
			meshes[21].isDraw = false; // 顔
	}

	// モデルレンダラー生成
	shaderParamWithMaterialName = {};
	AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMaterialName);

	// アニメーター生成
	anim = AddComponent<Animator>(model);
	anim->SetRootMotion("root");
	anim->Load("Data/Animator/Player.animator");

	// キャラクターコントローラ生成
	cc = AddComponent<CharacterController>(0.18f, 1.18f);
	cc->SetPosition({0, 0.6f, 0});
}

void Player::OnUpdate()
{
	if (!controller || !cc) return;

	InputContext ctx = controller->Poll();

	// ---- 入力ベクトルの長さ（0〜1） ----
	float inputLen = sqrtf(ctx.moveX * ctx.moveX + ctx.moveZ * ctx.moveZ);

	// Sprint 判定（入力がある && Shiftまたはスティック押し込み）
	bool sprinting = ctx.sprint && inputLen > 0.1f;

	// Speed: 停止=0 / 歩き=入力量(0〜0.8) / 走り=1.0 / スプリント=1.5
	float speedParam;
	if (inputLen < 0.1f)
		speedParam = 0.0f;
	else if (sprinting)
		speedParam = 1.5f;
	else
		speedParam = inputLen; // 0.1〜1.0

	// Animator パラメータを更新
	anim->SetFloat("Speed",       speedParam);
	anim->SetFloat("MoveX",       ctx.moveX);
	anim->SetBool ("IsSprinting", sprinting);

	// ---- プレイヤーの向きを入力方向に回転させる ----
	if (inputLen > 0.1f)
	{
		// moveX/Z はカメラ空間の入力と仮定（前方=+Z, 右=+X）
		// 入力方向のヨー角を求め、Quaternion に変換
		float targetYaw = atan2f(ctx.moveX, ctx.moveZ); // ラジアン
		Quaternion targetRot = Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);

		// 現在の向きから滑らかに回転（スラープ係数は好みで調整）
		float turnSpeed = sprinting ? 8.0f : 12.0f;
		float t = 1.0f - expf(-turnSpeed * Game::Time::deltaTime);
		transform.SetRotation(Quaternion::Slerp(transform.rotation, targetRot, t));
	}

	// ---- 重力 ----
	if (cc->IsGrounded())
		verticalVelocity = 0.0f;
	else
		verticalVelocity -= 9.81f * Game::Time::deltaTime;

	cc->Move({0, verticalVelocity * Game::Time::deltaTime, 0});
}

void Player::OnLateUpdate()
{
	// 1. ルートモーション差分を回収
	Vector3    localMoveVec = anim->GetRootMotionVec();
	Quaternion deltaRot     = anim->GetRootMotionRot();

	// 2. アニメーション由来の回転差分を適用（通常は歩き/走りには含まれないが念のため）
	transform.SetRotation(transform.rotation * deltaRot);

	// 3. ローカル移動ベクトルをプレイヤーの向きに合わせてワールド変換して移動
	Vector3 worldMoveVec = Vector3::Transform(localMoveVec, transform.rotation);
	cc->Move(worldMoveVec);
}

void Player::OnRender(const RenderContext& rc)
{
}

void Player::OnDrawGUI()
{
	// デバッグ情報
	ImGui::Text("State: %s", anim->GetCurrentStateName(0).c_str());
	ImGui::Text("Speed: %.2f", anim->GetFloat("Speed"));
}
