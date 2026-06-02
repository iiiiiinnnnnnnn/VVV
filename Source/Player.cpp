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
	//anim->Load("Data/Animator/CombatGirls_Sword_Shield.animator");

	// ルートモーションボーンを設定
	anim->SetRootMotionBone(116);

	// パラメータ登録
	anim->AddFloat("Speed");          // 0=Idle 0.5=Walk 1.0=Run 1.5=Sprint
	anim->AddBool("IsSprinting");

	int L0 = anim->AddLayer("Base", Animator::BlendMode::Override, 1.0f);

	// ステートを登録（animationIndex は ANIM() マクロで名前→Index変換）
	stIdle = anim->AddState(L0, "Idle", ANIM("SS_Idle"), true);
	stWalk = anim->AddState(L0, "Walk", ANIM("SS_Walk"), true);
	stRun = anim->AddState(L0, "Run", ANIM("SS_Run"), true);
	stSprint = anim->AddState(L0, "Sprint", ANIM("SS_Sprint"), true);

	// デフォルトをIdleに
	anim->SetDefaultState(L0, stIdle);

	// トランジション：Idle <-> Walk <-> Run <-> Sprint
	// --- Idle → Walk ---
	{
		int ti = anim->AddTransition(L0, stIdle, stWalk,
			/*duration*/0.2f, /*hasExitTime*/false);
		anim->AddCondition(L0, stIdle, ti,
			"Speed", Animator::ConditionMode::Greater, 0.1f);
	}
	// --- Walk → Idle ---
	{
		int ti = anim->AddTransition(L0, stWalk, stIdle,
			0.2f, false);
		anim->AddCondition(L0, stWalk, ti,
			"Speed", Animator::ConditionMode::Less, 0.1f);
	}
	// --- Walk → Run ---
	{
		int ti = anim->AddTransition(L0, stWalk, stRun,
			0.15f, false);
		anim->AddCondition(L0, stWalk, ti,
			"Speed", Animator::ConditionMode::Greater, 0.8f);
	}
	// --- Run → Walk ---
	{
		int ti = anim->AddTransition(L0, stRun, stWalk,
			0.15f, false);
		anim->AddCondition(L0, stRun, ti,
			"Speed", Animator::ConditionMode::Less, 0.8f);
	}
	// --- Run → Sprint ---
	{
		int ti = anim->AddTransition(L0, stRun, stSprint,
			0.1f, false);
		anim->AddCondition(L0, stRun, ti,
			"IsSprinting", Animator::ConditionMode::IsTrue);
	}
	// --- Sprint → Run ---
	{
		int ti = anim->AddTransition(L0, stSprint, stRun,
			0.1f, false);
		anim->AddCondition(L0, stSprint, ti,
			"IsSprinting", Animator::ConditionMode::IsFalse);
	}

	// --- CharacterController ---
	cc = AddComponent<CharacterController>(0.5f, 1.5f);
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

	// ルートモーション適用（既存のまま）
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
	ImGui::Text("State: %s", anim->GetCurrentStateName(0).c_str());
	ImGui::Text("Speed param: %.3f", anim->GetFloat("Speed"));
	ImGui::Text("LayerCount: %d", anim->GetLayerCount());
}