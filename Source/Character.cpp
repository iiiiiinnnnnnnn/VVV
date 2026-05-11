// Character.cpp

#include "Character.h"
#include "RemotePlayer.h"

void Character::SetSkin(uint32_t parts)
{
	auto& meshes = model->GetMeshes();
	meshes[0].isDraw = true;
	meshes[1].isDraw = parts & (uint32_t)SkinParts::Head_SoldierB;
	meshes[2].isDraw = parts & (uint32_t)SkinParts::Head_SoldierA;
	meshes[3].isDraw = parts & (uint32_t)SkinParts::Head;
	meshes[4].isDraw = parts & (uint32_t)SkinParts::Head_Brass;
	meshes[5].isDraw = parts & (uint32_t)SkinParts::Head_Officer;
	meshes[6].isDraw = parts & (uint32_t)SkinParts::Head_Medic;
	meshes[7].isDraw = parts & (uint32_t)SkinParts::Equip_Infantry;
	meshes[8].isDraw = parts & (uint32_t)SkinParts::Equip_Medic;
	meshes[9].isDraw = parts & (uint32_t)SkinParts::Body_Medic;
	meshes[10].isDraw = parts & (uint32_t)SkinParts::Head_GasMask;
	meshes[11].isDraw = parts & (uint32_t)SkinParts::Head_GasMask;
}

void Character::InitCharacter()
{
	// アニメーション読み込み
	{
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_combat_bayonet.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_combat_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_combat_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_combat_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_combat_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_combat_shoot_bolt.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_combat_shoot_burst.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_combat_shoot_shotgun.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_death_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_death_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_death_C.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_death_D.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_death_E.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_interact_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_interact_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/infantry_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_shoot_bolt.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_shoot_burst.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_shoot_shotgun.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/crouch/infantry_crouch_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/guard/infantry_guard_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/guard/infantry_guard_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/guard/infantry_guard_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_roll.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_run_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_run_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_run_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_jump_1_start.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_jump_2_air.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_jump_3_land.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_ladder_climb_down.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_ladder_climb_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_ladder_climb_up.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_sprint.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_death.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_goto.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_move.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_shoot_bolt.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_shoot_burst.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_shoot_shotgun.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_standup.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/prone/infantry_prone_throw_grenade.glb");
	}

	// キャラコン生成
	cc = AddComponent<CharacterController>(0.5f, 1.5f);

	// アニメーター生成
	anim = AddComponent<Animator>(model);

	// モデルレンダラー生成
	AddComponent<ModelRender>(model);

	// 手のノードを保存
	handNode = &model->GetNodes()[17];

	// 武器生成
	weapon = std::make_shared<Weapon>(this);

	// AvatarMask定義
	Animator::AvatarMask fullBody = {};  // 空=全身
	Animator::AvatarMask upperBody = { { 4,5,6,7, 8,9,10,11,12, 13,14,15,16,17, 18 } };

	// レイヤー追加
	int lBase = anim->AddLayer("Base", Animator::BlendMode::Override, 1.0f, fullBody);
	int lUpper = anim->AddLayer("Upper", Animator::BlendMode::Override, 1.0f, upperBody);

	// パラメータ（全レイヤーで共有）
	anim->AddFloat("speed");
	anim->AddBool("isCrouching");
	anim->AddTrigger("shoot");
	anim->AddTrigger("reload");

	// --- Base レイヤー ---
	int bIdle = anim->AddState(lBase, "Idle", ANIM("infantry_combat_idle"), true);
	int bWalk = anim->AddState(lBase, "Walk", ANIM("infantry_combat_walk"), true);
	int bRun = anim->AddState(lBase, "Run", ANIM("infantry_combat_run"), true);

	// Idle → Walk
	int t0 = anim->AddTransition(lBase, bIdle, bWalk, 0.1f);
	anim->AddCondition(lBase, bIdle, t0, "speed", Animator::ConditionMode::Greater, 0.1f);

	// Walk → Idle
	int t1 = anim->AddTransition(lBase, bWalk, bIdle, 0.1f);
	anim->AddCondition(lBase, bWalk, t1, "speed", Animator::ConditionMode::Less, 0.1f);

	// Walk → Run
	int t2 = anim->AddTransition(lBase, bWalk, bRun, 0.1f);
	anim->AddCondition(lBase, bWalk, t2, "speed", Animator::ConditionMode::Greater, 0.8f);

	// Run → Walk
	int t3 = anim->AddTransition(lBase, bRun, bWalk, 0.1f);
	anim->AddCondition(lBase, bRun, t3, "speed", Animator::ConditionMode::Less, 0.8f);

	// Run → Idle
	int t4 = anim->AddTransition(lBase, bRun, bIdle, 0.1f);
	anim->AddCondition(lBase, bRun, t4, "speed", Animator::ConditionMode::Less, 0.1f);

	anim->SetDefaultState(lBase, bIdle);

	// --- Upper レイヤー ---
	int uIdle = anim->AddState(lUpper, "Idle", ANIM("infantry_combat_idle"), true);
	int uShoot = anim->AddState(lUpper, "Shoot", ANIM("infantry_combat_shoot"), false);
	int uReload = anim->AddState(lUpper, "Reload", ANIM("infantry_combat_reload"), false);

	// Idle → Shoot/Reload
	int t5 = anim->AddTransition(lUpper, uIdle, uShoot, 0.05f, false, 1.0f, 10);
	anim->AddCondition(lUpper, uIdle, t5, "shoot", Animator::ConditionMode::Trigger);

	// Idle → Shoot/Reload
	int t6 = anim->AddTransition(lUpper, uIdle, uReload, 0.05f, false, 1.0f, 10);
	anim->AddCondition(lUpper, uIdle, t6, "reload", Animator::ConditionMode::Trigger);

	// Shoot/Reload → Idle はアニメ終端で戻る
	int t7 = anim->AddTransition(lUpper, uShoot, uIdle, 0.1f, true, 0.95f);
	int t8 = anim->AddTransition(lUpper, uReload, uIdle, 0.1f, true, 0.95f);

	anim->SetDefaultState(lUpper, uIdle);
}

void Character::OnUpdate(float elapsedTime)
{
	if (!controller) return;
	if (!cc) return;

	float moveX = controller->GetMoveX();
	float moveZ = controller->GetMoveZ();

	// 入力の大きさ(animatorに入れる)
	float inputLength = Vector3(moveX, 0, moveZ).Length();
	anim->SetFloat("speed", inputLength);

	if (controller->GetShoot())  anim->SetTrigger("shoot");
	if (controller->GetReload()) anim->SetTrigger("reload");

	// 移動ベクトル（speed メンバ変数で実際の速さをスケール）
	Vector3 move = Vector3::TransformNormal(
		Vector3(moveX, 0, moveZ),
		Matrix::CreateFromQuaternion(transform.rotation)
	);
	move *= speed * elapsedTime;  // speed は Character のメンバ変数(5.0f)

	if (cc->IsGrounded())
		verticalVelocity = 0.0f;
	else
		verticalVelocity -= 9.81f * elapsedTime;

	move.y = verticalVelocity * elapsedTime;
	cc->Move(move);

	if (weapon) weapon->Update(elapsedTime);
}

void Character::OnRender(const RenderContext& rc, float elapsedTime)
{
	if (weapon) {
		weapon->Render(rc, elapsedTime);
	}
}
