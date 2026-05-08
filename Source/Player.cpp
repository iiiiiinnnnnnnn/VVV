// Player.cpp

#include "Player.h"
#include <Graphics.h>
#include <GpuResourceUtils.h>
#include <Input.h>

Player::Player()
{
	// プレイヤー
	std::shared_ptr<Model> model = std::make_shared<Model>(
		"Data/Model/ToonSoldiers_WW2/models/ToonSoldier_WW2_Japan_Soldier.glb");

	// オプションのメッシュを非表示にする
	{
		model->GetMeshes()[0].isDraw = true;   // Japan_body_infantry
		model->GetMeshes()[1].isDraw = true;   // Japan_head_soldier_B
		model->GetMeshes()[2].isDraw = false;  // Japan_head_soldier_A
		model->GetMeshes()[3].isDraw = true;   // Japan_head
		model->GetMeshes()[4].isDraw = false;  // Japan_head_brass
		model->GetMeshes()[5].isDraw = false;  // Japan_head_officer
		model->GetMeshes()[6].isDraw = false;  // Japan_head_medic
		model->GetMeshes()[7].isDraw = false;  // Japan_equip_infantry
		model->GetMeshes()[8].isDraw = false;  // Japan_equip_medic
		model->GetMeshes()[9].isDraw = false;  // Japan_body_medic
		model->GetMeshes()[10].isDraw = false; // Japan_head_gasmask
		model->GetMeshes()[11].isDraw = false; // Japan_head_gasmask
	}

	// アニメーション追加
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
		/*model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/aim_poses/infantry_combat_aim_down.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/aim_poses/infantry_combat_aim_up.glb");*/
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
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_combat_roll.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_combat_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_combat_run_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_combat_run_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_combat_run_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_combat_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_combat_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_combat_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_combat_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_crouch_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_crouch_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_crouch_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_crouch_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_guard_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_guard_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_prone_move.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/root_motion/infantry_RM_sprint.glb");
	}

	// アニメーター生成
	auto anim = AddComponent<Animator>(model);
	anim->Play(2, true);

	// モデルレンダラー生成
	AddComponent<ModelRender>(model);

	// 手のノードを保存
	handNode = &model->GetNodes()[17];
}

void Player::OnUpdate(float elapsedTime)
{
	GamePad& gamePad = Input::Instance().GetGamePad();
	Mouse& mouse = Input::Instance().GetMouse();

	// 仮移動
	if (gamePad.GetButton() & gamePad.BTN_A) {
		transform.position += transform.forward * 0.1f;
	}
	// 仮回転
	if (gamePad.GetAxisLX() > 0.1f || gamePad.GetAxisLX() < -0.1f) {
		transform.rotation *= Quaternion::CreateFromAxisAngle(Vector3::UnitY, gamePad.GetAxisLX() * 0.05f);
	}
}

void Player::OnRender(const RenderContext& rc, float elapsedTime)
{

}
