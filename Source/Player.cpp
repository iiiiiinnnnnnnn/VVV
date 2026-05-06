// Player.cpp

#include "Player.h"
#include <Graphics.h>
#include <GpuResourceUtils.h>
#include <Input.h>

Player::Player() : AnimatedRenderActor("Data/Model/ToonSoldiers_WW2/models/ToonSoldier_WW2_Japan_Soldier.glb")
{
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
		/*model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_combat_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_combat_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_combat_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_combat_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_combat_shoot_burst.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_death_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_death_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_death_C.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_death_D.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_death_E.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_interact_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_interact_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/handgun_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/aim_poses/handgun_combat_aim_down.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/aim_poses/handgun_combat_aim_up.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_shoot_burst.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/crouch/handgun_crouch_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/guard/handgun_guard_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/guard/handgun_guard_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/guard/handgun_guard_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_combat_roll.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_combat_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_combat_run_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_combat_run_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_combat_run_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_combat_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_combat_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_combat_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_combat_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_jump_1_start.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_jump_2_air.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_jump_3_land.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_ladder_climb_down.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_ladder_climb_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_ladder_climb_up.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/movement/handgun_sprint.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_death.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_goto.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_move.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_shoot_burst.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_standup.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/prone/handgun_prone_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_combat_roll.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_combat_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_combat_run_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_combat_run_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_combat_run_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_combat_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_combat_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_combat_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_combat_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_crouch_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_crouch_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_crouch_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_crouch_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_guard_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_guard_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_prone_move.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Handgun/root_motion/handgun_RM_sprint.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_combat_attack_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_combat_attack_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_combat_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_combat_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_death_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_death_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_death_C.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_death_D.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_death_E.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_interact_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_interact_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/knife_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/aim_poses/knife_combat_aim_down.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/aim_poses/knife_combat_aim_up.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/crouch/knife_crouch_attack_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/crouch/knife_crouch_attack_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/crouch/knife_crouch_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/crouch/knife_crouch_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/crouch/knife_crouch_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/crouch/knife_crouch_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/crouch/knife_crouch_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/crouch/knife_crouch_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/crouch/knife_crouch_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/crouch/knife_crouch_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/guard/knife_guard_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/guard/knife_guard_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/guard/knife_guard_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_combat_roll.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_combat_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_combat_run_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_combat_run_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_combat_run_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_combat_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_combat_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_combat_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_combat_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_jump_1_start.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_jump_2_air.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_jump_3_land.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_ladder_climb_down.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_ladder_climb_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_ladder_climb_up.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/movement/knife_sprint.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/prone/knife_prone_attack.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/prone/knife_prone_death.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/prone/knife_prone_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/prone/knife_prone_goto.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/prone/knife_prone_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/prone/knife_prone_move.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/prone/knife_prone_standup.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/prone/knife_prone_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/prone/knife_prone_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_combat_roll.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_combat_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_combat_run_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_combat_run_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_combat_run_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_combat_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_combat_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_combat_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_combat_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_crouch_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_crouch_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_crouch_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_crouch_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_guard_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_guard_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_prone_move.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Knife/root_motion/knife_RM_sprint.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_combat_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_combat_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_combat_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_combat_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_combat_shoot_burst.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_combat_shoot_loop.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_death_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_death_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_death_C.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_death_D.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_death_E.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_interact_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_interact_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/machinegun_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/aim_poses/machinegun_combat_aim_down.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/aim_poses/machinegun_combat_aim_up.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_shoot_burst.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_shoot_loop.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/crouch/machinegun_crouch_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/guard/machinegun_guard_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/guard/machinegun_guard_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/guard/machinegun_guard_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_combat_roll.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_combat_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_combat_run_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_combat_run_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_combat_run_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_combat_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_combat_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_combat_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_combat_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_jump_1_start.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_jump_2_air.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_jump_3_land.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_ladder_climb_down.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_ladder_climb_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_ladder_climb_up.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/movement/machinegun_sprint.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_death.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_goto.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_move.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_shoot_burst.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_shoot_loop.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_standup.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/prone/machinegun_prone_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_combat_roll.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_combat_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_combat_run_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_combat_run_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_combat_run_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_combat_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_combat_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_combat_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_combat_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_crouch_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_crouch_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_crouch_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_crouch_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_guard_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_guard_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_prone_move.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Machinegun/root_motion/machinegun_RM_sprint.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_combat_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_combat_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_combat_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_combat_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_death_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_death_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_death_C.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_interact_A.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_interact_B.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/rocketlauncher_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/aim_poses/rocketlauncher_combat_aim_down.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/aim_poses/rocketlauncher_combat_aim_up.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/crouch/rocketlauncher_crouch_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/crouch/rocketlauncher_crouch_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/crouch/rocketlauncher_crouch_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/crouch/rocketlauncher_crouch_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/crouch/rocketlauncher_crouch_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/crouch/rocketlauncher_crouch_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/crouch/rocketlauncher_crouch_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/crouch/rocketlauncher_crouch_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/crouch/rocketlauncher_crouch_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/crouch/rocketlauncher_crouch_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/guard/rocketlauncher_guard_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/guard/rocketlauncher_guard_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/guard/rocketlauncher_guard_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_combat_roll.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_combat_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_combat_run_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_combat_run_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_combat_run_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_combat_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_combat_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_combat_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_combat_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_jump_1_start.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_jump_2_air.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_jump_3_land.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_ladder_climb_down.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_ladder_climb_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_ladder_climb_up.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/movement/rocketlauncher_sprint.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/prone/rocketlauncher_prone_death.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/prone/rocketlauncher_prone_draw.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/prone/rocketlauncher_prone_goto.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/prone/rocketlauncher_prone_idle.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/prone/rocketlauncher_prone_move.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/prone/rocketlauncher_prone_reload.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/prone/rocketlauncher_prone_shoot.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/prone/rocketlauncher_prone_standup.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/prone/rocketlauncher_prone_take_damage.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/prone/rocketlauncher_prone_throw_grenade.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_combat_roll.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_combat_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_combat_run_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_combat_run_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_combat_run_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_combat_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_combat_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_combat_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_combat_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_crouch_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_crouch_walk_back.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_crouch_walk_left.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_crouch_walk_right.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_guard_run.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_guard_walk.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_prone_move.glb");
		model->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/RocketLauncher/root_motion/rocketlauncher_RM_sprint.glb");*/
	}

	// アニメーター生成
	animator->Play(0, true);
}

void Player::Update(float elapsedTime)
{
	GamePad& gamePad = Input::Instance().GetGamePad();
	Mouse& mouse = Input::Instance().GetMouse();

	if (animator)
		animator->Update(elapsedTime);

	// 仮移動
	if (gamePad.GetButton() & gamePad.BTN_A) {
		transform.position += transform.forward * 0.1f;
	}
	// 仮回転
	if(gamePad.GetAxisLX() > 0.1f || gamePad.GetAxisLX() < -0.1f) {
		transform.rotation *= Quaternion::CreateFromAxisAngle(Vector3::UnitY, gamePad.GetAxisLX() * 0.05f);
	}

	transform.Update();
	model->UpdateTransform(transform.matrix);
}

void Player::Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer)
{
	// モデル描画
	renderer->Draw(ShaderId::Lambert, model);
	renderer->Render(rc);
}
