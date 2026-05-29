// Character.cpp

#include "Character.h"
#include "ResourceManager.h"

Character::Character(std::string name, std::string tag, bool isActive, std::string layer, Country country, SkinParts skinParts)
	: Actor(name, tag, isActive, layer), country(country)
{
	model = ResourceManager::Instance().LoadModel(GetModel(country));

	// スキン設定
	{
		auto& meshes = model->GetMeshes();
		meshes[0].isDraw = true;
		meshes[1].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Head_SoldierB;
		meshes[2].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Head_SoldierA;
		meshes[3].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Head;
		meshes[4].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Head_Brass;
		meshes[5].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Head_Officer;
		meshes[6].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Head_Medic;
		meshes[7].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Equip_Infantry;
		meshes[8].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Equip_Medic;
		meshes[9].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Body_Medic;
		meshes[10].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Head_GasMask;
		meshes[11].isDraw = (uint32_t)skinParts & (uint32_t)SkinParts::Head_GasMask;
	}

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
	shaderParamWithMeshName =
	{
		{
			"Japan_body_infantry",
			{
				{"metalness", 0.5f},
				{"roughness", 0.5f},
				{"occlusionStrength", 1.0f}
			}
		},
	};
	AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMeshName);

	// 手のノードを保存
	handNode = &model->GetNodes()[17];

	// 武器生成
	weapon = std::make_shared<Weapon>(this);

	anim->Load("Data/Animator/Character.animator");
}

void Character::OnUpdate(float elapsedTime)
{
	if (!controller) return;
	if (!cc) return;

	// 入力周り
	{
		float moveX = controller->GetMoveX();
		float moveZ = controller->GetMoveZ();

		// 入力の大きさ(animatorに入れる)
		float inputLength = Vector3(moveX, 0, moveZ).Length();
		anim->SetFloat("speed", inputLength);
		anim->SetBool("crouch", controller->GetCrouch());
		anim->SetBool("jump", controller->GetJump());
		anim->SetBool("ready", controller->GetReady());

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
	}
}

void Character::OnLateUpdate(float elapsedTime)
{
	// スパインの回転
	if (isFirstPerson)
	{
		Vector2 targetSpineAngleX = controller->GetReady() ? readySpineAngle : idleSpineAngle;

		Model::Node* spineNode = &model->GetNodes()[4];
		Quaternion baseRot = spineNode->rotation;
		Quaternion aimRot = Quaternion::CreateFromAxisAngle(Vector3::UnitZ, -spineAngleX + targetSpineAngleX.x);
		Quaternion aimRot2 = Quaternion::CreateFromAxisAngle(Vector3::UnitX, targetSpineAngleX.y);
		spineNode->rotation = baseRot * aimRot * aimRot2;
		model->UpdateTransform(transform.matrix);
	}

	// 武器
	if (weapon) weapon->Update(elapsedTime);
}

void Character::OnRender(const RenderContext& rc, float elapsedTime)
{
	if (weapon) {
		weapon->Render(rc, elapsedTime);
	}
}

void Character::OnDrawGUI(float elapsedTime)
{
	ImGui::PushID(this);
	ImGui::TreePush("Character");
	if (weapon) {
		weapon->OnDrawGUI(elapsedTime);
	}
	ImGui::TreePop();
	ImGui::PopID();
}

std::string Character::GetModel(Country type)
{
	switch (type)
	{
		case Country::Japan:	return "Data/Model/ToonSoldiers_WW2/models/ToonSoldier_WW2_Japan_Soldier.glb";
		case Country::US:		return "Data/Model/ToonSoldiers_WW2/models/ToonSoldier_WW2_US_Soldier.glb";
		case Country::German:	return "Data/Model/ToonSoldiers_WW2/models/ToonSoldier_WW2_German_Soldier.glb";
		case Country::Soviet:	return "Data/Model/ToonSoldiers_WW2/models/ToonSoldier_WW2_Soviet_Soldier.glb";
		case Country::British:	return "Data/Model/ToonSoldiers_WW2/models/ToonSoldier_WW2_British_Soldier.glb";
		default:				return "";
	}
}
