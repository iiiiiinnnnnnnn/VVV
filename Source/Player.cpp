// Player.cpp

#include "Player.h"
#include "ResourceManager.h"
#include "ThirdPersonCameraController.h"
#include "GameTime.h"

/*Anim: SS_Attack1, length=1.50
Anim: SS_Attack2, length=2.08
Anim: SS_Attack3, length=1.50
Anim: SS_Attack4, length=1.83
Anim: SS_CrouchIdle, length=2.21
Anim: SS_CrouchJog, length=0.83
Anim: SS_CrouchWalk, length=1.12
Anim: SS_Die, length=2.00
Anim: SS_Evade, length=1.33
Anim: SS_Hit_L, length=1.46
Anim: SS_Hit_R, length=1.46
Anim: SS_Idle, length=3.00
Anim: SS_Put, length=2.00
Anim: SS_Quickshift_B, length=1.00
Anim: SS_Quickshift_F, length=1.00
Anim: SS_Quickshift_L, length=1.00
Anim: SS_Quickshift_R, length=1.00
Anim: SS_Run, length=0.67
Anim: SS_SP_Idle, length=3.00
Anim: SS_SP_Run, length=0.83
Anim: SS_Sp_Skill1, length=2.33
Anim: SS_Sp_Skill2, length=2.12
Anim: SS_Sp_Skill3, length=2.50
Anim: SS_Sp_TurnL, length=0.67
Anim: SS_Sp_TurnR, length=0.67
Anim: SS_SP_Walk, length=1.12
Anim: SS_Sprint, length=0.58
Anim: SS_Stun, length=2.00
Anim: SS_Take, length=3.67
Anim: SS_Walk, length=1.12*/

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
	anim->_print(); // デバッグ用：アニメーターの内容をコンソールに表示

	// キャラクターコントローラ生成
	cc = AddComponent<CharacterController>(0.18f, 1.18f);
	cc->SetPosition({0, 0.6f, 0});
}

void Player::OnUpdate()
{
	InputContext ctx = controller->Poll();

	// ---- 入力ベクトルをカメラYaw基準のワールド方向に変換 ----
	float inputLen = sqrtf(ctx.moveX * ctx.moveX + ctx.moveZ * ctx.moveZ);
	const std::string currentStateName = anim ? anim->GetCurrentStateName(0) : "";
	const bool isAttackState = (currentStateName.find("Attack") != std::string::npos);

	Vector3 worldMoveDir = Vector3::Zero;
	if (inputLen > 0.1f)
	{
		float camYaw = cameraController ? cameraController->GetCameraYaw() : 0.0f;

		// カメラのYaw回転行列でローカル入力をワールド方向へ
		float sinY = sinf(camYaw);
		float cosY = cosf(camYaw);

		// 入力(moveX=右, moveZ=前) をカメラ基準でワールドXZ に変換
		worldMoveDir.x = ctx.moveX * cosY + ctx.moveZ * sinY;
		worldMoveDir.z = ctx.moveX * (-sinY) + ctx.moveZ * cosY;
		worldMoveDir.Normalize();

		// 攻撃中は入力による方向転換を止める
		if (!isAttackState)
		{
			bool sprinting = ctx.sprint && inputLen > 0.1f;
			float turnSpeed = sprinting ? 8.0f : 12.0f;
			float targetYaw = atan2f(worldMoveDir.x, worldMoveDir.z);
			Quaternion targetRot = Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
			float t = 1.0f - expf(-turnSpeed * Game::Time::deltaTime);
			transform.SetRotation(Quaternion::Slerp(transform.rotation, targetRot, t));
		}
	}

	// Speed / Sprint パラメータをAnimatorへ
	bool sprinting = ctx.sprint && inputLen > 0.1f;
	float speedParam = (inputLen < 0.1f) ? 0.0f : (sprinting ? 1.5f : inputLen);
	anim->SetFloat("Speed",       speedParam);
	anim->SetBool ("IsSprinting", sprinting);

	if (ctx.attackPressed)
		anim->SetTrigger("Attack");

	// 重力
	if (cc->IsGrounded())
		verticalVelocity = 0.0f;
	else
		verticalVelocity -= 9.81f * Game::Time::deltaTime;

	cc->Move({0, verticalVelocity, 0});
}

void Player::OnLateUpdate()
{
	// ルートモーション差分を回収
	Vector3    localMoveVec = anim->GetRootMotionVec();
	Quaternion deltaRot     = anim->GetRootMotionRot();

	// アニメーション由来の回転差分を適用（通常は歩き/走りには含まれないが念のため）
	transform.SetRotation(transform.rotation * deltaRot);

	// ローカル移動ベクトルをプレイヤーの向きに合わせてワールド変換して移動
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
