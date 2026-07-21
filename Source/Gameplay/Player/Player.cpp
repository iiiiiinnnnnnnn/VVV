// Player.cpp

#include "Gameplay/Player/Player.h"
#include "Resource/ResourceManager.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Application/Time/GameTime.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Actor/ActorManager.h"

Player::Player() : Entity("Player", "Player", true, 100.0f, 100.0f)
{
	model = ResourceManager::Instance().LoadModel("Data/Model/CombatGirl_Shield/CombatGirls_Sword_Shield");
	model->_print(); // デバッグ用

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
			{"metalness", 0.0f},
		{"roughness", 0.0f},
		{"occlusion", 0.0f},
		{"occlusionStrength", 1.0f},
		{"shadowStrength", 0.5f}
	}
		},
		{
			"Eye",
		{
			{"metalness", 0.0f},
		{"roughness", 0.0f},
		{"occlusion", 1.0f},
		{"occlusionStrength", 1.0f},
		{"shadowStrength", 0.5f}
	}
		},
		{
			"Body",
		{
			{"metalness", 0.0f},
		{"roughness", 0.0f},
		{"occlusion", 0.0f},
		{"occlusionStrength", 1.0f},
		{"shadowStrength", 0.5f}
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
	modelRenderer = AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMaterialName);
	modelRenderer->SetAutoUpdateTransform(false);

	// アニメーター生成
	anim = AddComponent<Animator>(model);
	anim->SetRootMotion("root");
	anim->Load("Data/Animator/Player.animator");
	anim->AddCallbackFunc("OnAnim", [this](const Animator::State& s) { OnEnterAnim(s); }, [this](const Animator::State& s) { OnExitAnim(s); });
	anim->AddCallbackFunc("OnAttack4B", [this](const Animator::State& s) { OnEnterAnimAttack4B(s); }, [this](const Animator::State& s) { OnExitAnimAttack4B(s); });
	anim->BindCallbacks();

	// キャラクターコントローラ生成
	float radius = 0.25f;
	float totalHeight = 1.7f;
	float capsuleHeight = totalHeight - radius * 2.0f;

	cc = AddComponent<CharacterController>(
		Layers::Get("Player"),
		radius,
		capsuleHeight
	);
	cc->SetUseGravity(false);
	cc->SetStepOffset(0.15f);
	cc->SetSlopeLimitDeg(70.0f);
	cc->SetContactOffset(0.05f);
	cc->SetOwnerAnchorAtCenter(false);
	cc->SetOwnerAnchorOffsetY(0.0f);

	// SetFootPosition と SetPosition は両方呼ばない
	cc->SetFootPosition({ 0.0f, 5.0f, 10.0f });

	// 武器判定
	weaponCollider = AddComponent<BoneSphereCollider>(
		Layers::Get("PlayerAtk"),
		model.get(),
		model->GetNodeIndex("add_weapon_r"),
		0.5f,
		Matrix::CreateTranslation({-0.56f, 0, 0}));
	weaponCollider->SetActive(false);

	// キック判定
	footCollider = AddComponent<BoneSphereCollider>(
		Layers::Get("PlayerAtk"),
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
		Layers::Get("Hair"), 
		model.get(),
		std::vector<std::string>{"hair"},
		std::vector<SpringBone::SpringCapsule>(
		{
			{Vector3::Zero, {0.0f, 0.10f, 0.0f}, 0.076f, model->GetNodeIndex("head")},
			{Vector3::Zero, {0.0f, 0.10f, 0.0f}, 0.130f, model->GetNodeIndex("spine_01")},
			{Vector3::Zero, {0.0f, 0.10f, 0.0f}, 0.155f, model->GetNodeIndex("spine_02")},
			{Vector3::Zero, {0.0f, 0.16f, 0.0f}, 0.100f, model->GetNodeIndex("spine_03")}
		})
	);

	// FootIK
	footIK = AddComponent<HumanoidFootIK>(
		Layers::Get("Foot"),
		model.get(),
		anim,
		"Idle",
		"pelvis",
		"thigh_l", "calf_l", "foot_l", "ball_l",
		"thigh_r", "calf_r", "foot_r", "ball_r");
	model->UpdateTransform(transform.matrix);

	// LookAt
	lookAt = AddComponent<LookAt>(
		model.get(), "head", "neck_01");
	lookAt->SetActive(false);
}

void Player::OnUpdate()
{
	Entity::OnUpdate();

	UpdateLookIn();
	UpdateMovement();
}

void Player::OnLateUpdate()
{
	Vector3 localMoveVec = anim->GetRootMotionVec();
	Quaternion deltaRot = anim->GetRootMotionRot();

	transform.SetRotation(transform.rotation * deltaRot);

	Vector3 worldMoveVec = Vector3::Transform(localMoveVec, transform.rotation);
	worldMoveVec += knockBackVelocity * Game::Time::deltaTime;
	worldMoveVec.y += verticalVelocity * Game::Time::deltaTime;

	cc->Move(worldMoveVec);
	SnapToGroundIfNeeded();

	model->UpdateTransform(GetModelWorldTransform());

	// 最終姿勢が決まった後に、武器ノードを同期する
	SyncWeaponAttachNodes();

	if (cc)
	{
		cc->ClearDebugRenderPosition();
	}
}

void Player::OnDrawGUI()
{
	Entity::OnDrawGUI();

	bool changed = false;

	changed |= ImGui::DragFloat("groundSnapUpDistance", &groundSnapUpDistance, 0.01f, 0.0f, 2.0f);
	changed |= ImGui::DragFloat("groundSnapDownDistance", &groundSnapDownDistance, 0.01f, 0.0f, 3.0f);

	if (changed && cc)
	{
		cc->SetPosition(transform.position);
	}

	ImGui::Text("LookIn Target: %s", lookInTarget ? lookInTarget->GetName().c_str() : "None");
	ImGui::Text("LookAt Target: %.1f,%.1f,%.1f", lookAt->GetTarget().x, lookAt->GetTarget().y, lookAt->GetTarget().z);
}

void Player::OnDamaged(const DamageData& damageData)
{
	CameraEffectController::Request(0.13f, 0.07f);
	float lifeIntensity = (1 - (life / maxLife)) * 0.5f;
	PostProcessController::Instance().RequestDamagedVignette(
		5.0f * lifeIntensity, 3.0f * lifeIntensity, 0.15f, Easing::Type::InSine, Easing::Type::OutCubic);

	if (damageData.hitPosition.has_value())
	{
		// 敵の位置に応じてアニメーション再生
		Vector3 dir = damageData.hitPosition.value() - transform.position;
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

void Player::OnDead(const DamageData& damageData)
{

}

void Player::OnEnterAnim(const Animator::State& state)
{
	if (state.name.starts_with("Attack"))
	{
		weaponCollider->SetActive(true);
		trail->StartTrail();
	}
}

void Player::OnExitAnim(const Animator::State& state)
{
	if (state.name.starts_with("Attack"))
	{
		weaponCollider->SetActive(false);
		trail->StopTrail();
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

void Player::OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	//printf("OnCollisionEnter: %s\n", other->GetName().c_str());
}

void Player::OnCollisionStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	//printf("OnCollisionStay: %s\n", other->GetName().c_str());
}

void Player::OnCollisionExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	//printf("OnCollisionExit: %s\n", other->GetName().c_str());
}

void Player::OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (!self || !other) return;
	if (!self->IsActive()) return;
	if (self != weaponCollider && self != footCollider) return;

	// 敵を殴る

	Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());
	if (!otherActor->CompareTag("Enemy") && !otherActor->CompareTag("CrystalProp")) return;

	Entity* entity = dynamic_cast<Entity*>(otherActor);
	if (!entity) return;

	bool footAtk = self == footCollider;
	Vector3 hitPosition = point;
	Vector3 hitNormal = normal;
	BoneSphereCollider* attackCollider = footAtk ? footCollider : weaponCollider;
	Vector3 rayOrigin = attackCollider->GetWorldPosition();
	Vector3 rayDirection = point - rayOrigin;
	if (rayDirection.LengthSquared() <= eps)
		rayDirection = otherActor->transform.position - rayOrigin;
	if (rayDirection.LengthSquared() > eps)
	{
		float rayDistance = rayDirection.Length() * 2.0f + 2.0f;
		rayDirection.Normalize();

		PhysicsManager::PhysicsRaycastHit hit;
		if (PhysicsManager::Instance().Raycast(
			rayOrigin,
			rayDirection,
			rayDistance,
			hit,
			self->GetLayerId(),
			this))
		{
			hitPosition = hit.position;
			hitNormal = -hit.normal;
		}
	}

	entity->TakeDamage({
		.damage = footAtk ? Random::Range(45.0f, 55.0f) : Random::Range(30.0f, 40.0f),
		.knockBackPower = footAtk ? 80.0f : 50.0f,
		.hitColliderSelf = self,
		.hitColliderOther = other,
		.hitPosition = hitPosition,
		.hitNormal = hitNormal,
		});
}

void Player::OnTriggerStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	//printf("OnTriggerStay: %s\n", other->GetName().c_str());
}

void Player::OnTriggerExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	//printf("OnTriggerExit: %s\n", other->GetName().c_str());
}

void Player::SyncWeaponAttachNodes()
{
	if (!model) return;

	const int weaponL = model->GetNodeIndex("add_weapon_l");
	const int weaponR = model->GetNodeIndex("add_weapon_r");
	const int handL   = model->GetNodeIndex("hand_l");
	const int handR   = model->GetNodeIndex("hand_r");

	if (weaponL < 0 || weaponR < 0 || handL < 0 || handR < 0) return;

	model->GetNodes()[weaponL].worldTransform =
		model->GetNodes()[handL].worldTransform;

	model->GetNodes()[weaponR].worldTransform =
		model->GetNodes()[handR].worldTransform;
}

Matrix Player::GetModelWorldTransform() const
{
	return transform.matrix;
}

bool Player::RaycastGround(PhysicsManager::PhysicsRaycastHit& hit) const
{
	Vector3 rayStart =
		transform.position + Vector3(0.0f, groundSnapUpDistance, 0.0f);

	float rayDistance =
		groundSnapUpDistance + groundSnapDownDistance;

	return PhysicsManager::Instance().Raycast(
		rayStart,
		Vector3::Down,
		rayDistance,
		hit,
		Layers::Get("Player"),
		this
	);
}

void Player::SnapToGroundIfNeeded()
{
	PhysicsManager::PhysicsRaycastHit hit;
	groundedByRay = RaycastGround(hit);

	if (!groundedByRay) return;
	if (hit.normal.y < 0.35f) return;
	if (verticalVelocity > 0.0f) return;

	Vector3 targetPosition = transform.position;
	targetPosition.y = hit.position.y;

	float t = 1.0f - expf(-30.0f * Game::Time::deltaTime);
	t = std::clamp(t, 0.0f, 1.0f);

	transform.SetPosition(Vector3::Lerp(transform.position, targetPosition, t));

	if (cc)
	{
		cc->SetOwnerAnchorOffsetY(0.0f);
		cc->SetPosition(transform.position);
	}

	verticalVelocity = 0.0f;
}

void Player::UpdateLookIn()
{
	ActorManager* actorManager = ActorManager::GetActive();
	if (!actorManager) return;
	auto actors = actorManager->GetActors();
	bool found = false;
	float foundDistance = 500.0f;
	const float lockInDistance = 20.0f;
	for (Actor* actor : actors)
	{
		if (!actor) continue;
		if (actor == this) continue;
		if (!actor->CompareTag("Enemy")) continue;

		float dist = Vector3::Distance(
			actor->transform.position, transform.position);
		if (dist < lockInDistance)
		{
			found = true;
			if (dist < foundDistance)
			{
				foundDistance = dist;
				lookInTarget = actor;
			}
		}
	}
	if (found)
		lookAt->SetTarget(lookInTarget->transform.position);
	else
		lookInTarget = nullptr;
	lookAt->SetActive(found);
}

void Player::UpdateMovement()
{
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
	anim->SetFloat("Speed", speedParam);
	anim->SetBool("IsSprinting", sprinting);
	anim->SetBool("IsDead", IsDead());

	if (ctx.attackPressed)
		anim->SetTrigger("Attack");

	PhysicsManager::PhysicsRaycastHit groundHit;
	groundedByRay = RaycastGround(groundHit);

	// 重力
	if (groundedByRay && verticalVelocity <= 0.0f)
		verticalVelocity = 0.0f;
	else
		verticalVelocity -= 9.81f * Game::Time::deltaTime;

	frameVelocity.y = verticalVelocity * Game::Time::deltaTime;
}
