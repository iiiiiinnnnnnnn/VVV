// Player.cpp

#include "Player.h"
#include "ResourceManager.h"
#include "ThirdPersonCameraController.h"
#include "GameTime.h"
#include "Graphics.h"
#include "HitEffect.h"

Player::Player() : Entity("Player", "Player", true, Layer::Player, 100.0f, 100.0f)
{
	model = ResourceManager::Instance().LoadModel("Data/Model/CombatGirl_Shield/CombatGirls_Sword_Shield.glb");
	model->_print(); // デバッグ用

	// 武器がずれるやつ修正
	{
		model->AttachNodeToNode(
			model->GetNodeIndex("add_weapon_l"),
			model->GetNodeIndex("hand_l"));
		model->AttachNodeToNode(
			model->GetNodeIndex("add_weapon_r"),
			model->GetNodeIndex("hand_r"));
	}

	// メッシュ表示/非表示
	{
		auto& meshes = model->GetMeshes();
		meshes[0].isDraw = false; // 盾
		meshes[2].isDraw = false; // アックス
		meshes[8].isDraw = false;// 素足
		meshes[15].isDraw = false; // 私服
		meshes[9].isDraw = false; // 素手
		meshes[4].isDraw =  // 顔
			meshes[5].isDraw =
			meshes[16].isDraw =
			meshes[17].isDraw =
			meshes[18].isDraw =
			meshes[19].isDraw =
			meshes[20].isDraw =
			meshes[21].isDraw = false;
	}

	// モデルレンダラー生成
	shaderParamWithMaterialName =
	{
		{
			"Weapon_Axe_Shiled",
		{
			{"metalness", 0.8f},
		{"roughness", 0.0f},
		{"occlusion", 0.5f},
		{"occlusionStrength", 1.0f}
	}
		},
		{
			"Weapon_Sword_Shiled",
		{
			{"metalness", 0.8f},
		{"roughness", 0.0f},
		{"occlusion", 0.5f},
		{"occlusionStrength", 1.0f}
	}
		},
		{
			"Face",
		{
			{"isFace", true},
		{"metalness", 0.0f},
		{"roughness", 0.0f},
		{"occlusion", 0.0f},
		{"occlusionStrength", 1.0f}
	}
		},
		{
			"Eye",
		{
			{"metalness", 0.0f},
		{"roughness", 0.0f},
		{"occlusion", 1.0f},
		{"occlusionStrength", 1.0f}
	}
		},
		{
			"Body",
		{
			{"metalness", 0.0f},
		{"roughness", 0.0f},
		{"occlusion", 0.0f},
		{"occlusionStrength", 1.0f}
	}
		},
		{
			"Shiled_Hair",
		{
			{"metalness", 0.0f},
		{"roughness", 0.0f},
		{"occlusion", 0.0f},
		{"occlusionStrength", 1.0f}
	}
		},
		{
			"Shiled_Cloth",
		{
			{"metalness", 0.1f},
		{"roughness", 0.0f},
		{"occlusion", 0.0f},
		{"occlusionStrength", 0.8f}
	}
		},
		{
			"Squire_Cloth",
		{
			{"metalness", 0.0f},
		{"roughness", 0.0f},
		{"occlusion", 0.0f},
		{"occlusionStrength", 0.08f}
	}
		},
	};
	AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMaterialName);

	// アニメーター生成
	anim = AddComponent<Animator>(model);
	anim->SetRootMotion("root");
	anim->Load("Data/Animator/Player.animator");
	anim->AddCallbackFunc("OnAnim", [this](const Animator::State& s) { OnEnterAnim(s); }, [this](const Animator::State& s) { OnExitAnim(s); });
	anim->AddCallbackFunc("OnAttack4B", [this](const Animator::State& s) { OnEnterAnimAttack4B(s); }, [this](const Animator::State& s) { OnExitAnimAttack4B(s); });
	anim->BindCallbacks();

	// キャラクターコントローラ生成
	cc = AddComponent<CharacterController>(0.3f, 0.9f);
	cc->SetPosition({0, 2.0f, 0});

	// 武器判定
	weaponCollider = AddComponent<BoneSphereCollider>(
		model.get(),
		model->GetNodeIndex("add_weapon_r"),
		0.5f,
		Matrix::CreateTranslation({-0.56f, 0, 0}));
	weaponCollider->SetActive(false);

	// キック判定
	footCollider = AddComponent<BoneSphereCollider>(
		model.get(),
		model->GetNodeIndex("foot_l"),
		0.5f,
		Matrix::CreateTranslation({0, 0, 0}));
	footCollider->SetActive(false);

	// Trail
	trail = AddComponent<TrailRenderComponent>(
		model.get(),
		model->GetNodeIndex("add_weapon_r"));
	trail->StopTrail();

	// HairPhysics
	hairSpringBone = AddComponent<SpringBone>(
		model.get(),
		std::vector<std::string>{"Hair", "hair"}
	);
}

void Player::OnEnterAnim(const Animator::State& state)
{
	if (state.name.compare("Attack"))
	{
		weaponCollider->SetActive(true);
	}
}

void Player::OnExitAnim(const Animator::State& state)
{
	if (state.name.compare("Attack"))
	{
		weaponCollider->SetActive(false);
	}
}

void Player::OnEnterAnimAttack4B(const Animator::State& state)
{
	footCollider->SetActive(true);
}

void Player::OnExitAnimAttack4B(const Animator::State& state)
{
	footCollider->SetActive(false);
}

void Player::OnCollisionEnter(Actor* other)
{
	//printf("OnCollisionEnter: %s\n", other->GetName().c_str());
}

void Player::OnCollisionStay(Actor* other)
{
	//printf("OnCollisionStay: %s\n", other->GetName().c_str());
}

void Player::OnCollisionExit(Actor* other)
{
	//printf("OnCollisionExit: %s\n", other->GetName().c_str());
}

void Player::OnTriggerEnter(Actor* other)
{
	if (other->CompareTag("Enemy"))
	{
		Entity* entity = static_cast<Entity*>(other);
		bool footAtk = footCollider->IsActive();
		entity->TakeDamage(
			footAtk ? Random::Range(45.0f, 55.0f) : Random::Range(30.0f, 40.0f),
			{this, 50.0f});
	}
}

void Player::OnTriggerStay(Actor* other)
{
	//printf("OnTriggerStay: %s\n", other->GetName().c_str());
}

void Player::OnTriggerExit(Actor* other)
{
	//printf("OnTriggerExit: %s\n", other->GetName().c_str());
}

void Player::OnUpdate()
{
	Entity::OnUpdate();

	if (!controller) return;

	InputContext ctx = controller->Poll();

	// ---- 入力ベクトルをカメラYaw基準のワールド方向に変換 ----
	float inputLen = sqrtf(ctx.moveX * ctx.moveX + ctx.moveZ * ctx.moveZ);
	const std::string currentStateName = anim ? anim->GetCurrentStateName(0) : "";
	const bool isFreeze = (currentStateName.find("Freeze") != std::string::npos); // 動けない

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
		if (!isFreeze)
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
	anim->SetBool ("IsDead", IsDead());

	if (ctx.attackPressed)
		anim->SetTrigger("Attack");

	// 重力
	if (cc->IsGrounded())
		verticalVelocity = 0.0f;
	else
		verticalVelocity -= 9.81f * Game::Time::deltaTime;

	frameVelocity.y = verticalVelocity * Game::Time::deltaTime;

	// trail
	if(weaponCollider->IsActive())
		trail->StartTrail();
	else
		trail->StopTrail();
}

void Player::OnLateUpdate()
{
	Vector3    localMoveVec = anim->GetRootMotionVec();
	Quaternion deltaRot = anim->GetRootMotionRot();
	transform.SetRotation(transform.rotation * deltaRot);

	Vector3 worldMoveVec = Vector3::Transform(localMoveVec, transform.rotation);
	worldMoveVec += knockBackVelocity * Game::Time::deltaTime;
	worldMoveVec.y += verticalVelocity * Game::Time::deltaTime;
	cc->Move(worldMoveVec);
}

void Player::OnDrawGUI()
{
	Entity::OnDrawGUI();
}

void Player::OnDamaged(float damage, KnockBackData knockBackData)
{
	CameraShake::Request(0.13f, 0.07f);
	DamageVignette::Request(3.0f * (1 - (life / maxLife)));

	if (knockBackData.HasData())
	{
		// 敵の位置に応じてアニメーション再生
		Vector3 dir = knockBackData.GetSource()->transform.position - transform.position;
		dir.Normalize();

		float dot = transform.right.Dot(dir);

		if (dot >= 0.0f)
		{
			anim->SetTrigger("Hit_R");
		}
		else
		{
			anim->SetTrigger("Hit_L");
		}
	}
	else
	{
		anim->SetTrigger("Hit_R");
	}
}

void Player::OnDead()
{
}
