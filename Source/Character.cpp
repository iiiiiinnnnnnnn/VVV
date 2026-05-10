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
}

void Character::OnUpdate(float elapsedTime)
{
	if (!controller) return;
	if (!cc) return;

	float moveX = controller->GetMoveX();
	float moveZ = controller->GetMoveZ();

	Vector3 move = Vector3::TransformNormal(
		Vector3(moveX, 0, moveZ),
		Matrix::CreateFromQuaternion(transform.rotation)
	);
	move *= speed * elapsedTime;

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
