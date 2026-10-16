#include "Deer.h"
#include "Animation/MultiLegFootIK.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Audio/SoundSystem.h"

Deer::Deer(const Transform& transform, const std::string& modelPath, bool inBossField)
	: Entity(inBossField ? "BossFieldDeer" : "Deer", "Enemy", true, transform, 200.0f, 200.0f)
{
	vmdl = AddComponent<VMDL>(modelPath);
	anim = vmdl->GetAnimator();
	model = vmdl->GetSharedModel();
	vmdl->SetAutoUpdateTransform(false);

	auto footIK = vmdl->GetMultiLegFootIK();
	footIK->SetContactOffset(-0.864f);
	footIK->SetModelVisualOffsetY(-0.22f);
	anim->Load("Resources/Animator/Deer.animator");
	anim->SetRootMotion("armature");

	// キャラクターコントローラー
	cc = AddComponent<CharacterController>(
		Layers::Get("Enemy"), 0.39f, 1.23f);
	cc->SetStepOffset(0.15f);
	cc->SetSlopeLimitDeg(70.0f);
	cc->SetContactOffset(0.1f);

	/*motor = AddComponent<CharacterMotorComponent>(anim, cc);
	motor->SetRootMotionNode("armature");
	motor->SetUseRootMotion(false);*/

	// NavMeshAgent
	navMeshAgent = AddComponent<NavMeshAgent>();
	navMeshAgent->SetStoppingDistance(0.8f);
	navMeshAgent->SetTurnSpeed(1.0f);

	// LookAt
	auto lookAt = AddComponent<LookAt>(model.get(), "spine", "neck");
	lookAt->SetFilterTags({"Player", "Enemy"});
	lookAt->SetLookDistance(8.0f);

	// EnemyAI
	ConfigureAI();

}

void Deer::ConfigureAI()
{
	controller = AddComponent<EnemyAI>();
	controller->Load("Resources/AI/Deer.lua");

	controller->SetTrackingTurnSpeed(1.0f);

	// FootIK
	auto ik = vmdl->GetMultiLegFootIK();
	ik->SetModelVisualOffsetY(-0.07f);
	ik->SetContactOffset(-0.044f);

	// 共通：移動停止
	const auto stop = [this]()
	{
		StopCharge();
	};

	controller->AddAction("FreedomWait", stop);
	controller->AddAction("Stop", stop);

	// 非戦闘：構えを解除する。
	const auto updateFreedom = [this]()
	{
		anim->SetBool("ready", false);
	};

	controller->AddAction("Freedom", stop, updateFreedom, {}, stop);

	// 戦闘：突進に備えて構える。
	const auto updateAttacking = [this]()
	{
		anim->SetBool("ready", true);
	};

	controller->AddAction("Attacking", stop, updateAttacking, {}, stop);

	// 徘徊：ランダムな目的地へ歩く。
	const auto enterWander = [this]()
	{
		navMeshAgent->SetSpeed(controller->GetFloat("FreedomMoveSpeed", 1.0f));
		navMeshAgent->MoveToRandomPosition(
			controller->GetFloat("FreedomMinDistance", 3.0f),
			controller->GetFloat("FreedomMaxDistance", 10.0f));
	};

	controller->AddAction("FreedomWander", enterWander);

	// 旋回：移動を止め、向きを合わせる。
	const auto updateTurn = [this]()
	{
		const float angle = controller->GetFloat("TurnAngle");
		anim->SetBool("turnR", angle > 0.0f);
		anim->SetBool("turnL", angle < 0.0f);
		navMeshAgent->SetMovementPaused(true);
		if (!navMeshAgent->HasDestination()) controller->FaceTarget();
	};

	const auto exitTurn = [this]()
	{
		anim->SetBool("turnR", false);
		anim->SetBool("turnL", false);
		navMeshAgent->SetMovementPaused(false);
	};

	controller->AddAction("Turn", {}, updateTurn, {}, exitTurn);

	// 突進準備：プレイヤーの方向を向く。
	const auto updateFaceTarget = [this]()
	{
		controller->FaceTarget();
	};

	controller->AddAction("FaceTarget", {}, updateFaceTarget);

	// 突進：開始時の方向に固定して直進する。
	const auto enterChase = [this]()
	{
		Actor* target = controller->GetTarget();
		if (!target)
		{
			StopCharge();
			return;
		}

		Vector3 over = target->transform.position - this->transform.position;
		over.y = 0.0f;
		if (over.LengthSquared() <= eps)
		{
			StopCharge();
			return;
		}
		over.Normalize();
		chargeDirection = over;
		normalSlopeLimit = cc->GetSlopeLimitDeg();
		cc->SetSlopeLimitDeg(45.0f);
		charging = true;
		this->transform.SetRotation(Quaternion::CreateFromYawPitchRoll(
			atan2f(over.x, over.z), 0.0f, 0.0f));

		const float overDistance = controller->GetFloat("AttackOverDistance", 16.0f);
		const float moveSpeed = controller->GetFloat("AttackMoveSpeed", 6.0f);
		const Vector3 chargeOffset = over * overDistance;
		const Vector3 destination = target->transform.position + chargeOffset;

		navMeshAgent->SetSpeed(moveSpeed);
		navMeshAgent->SetChargeMovement(true);
		navMeshAgent->MoveToPosition(destination);
	};

	const auto updateChase = [this]()
	{
		Actor* target = controller->GetTarget();
		if (!charging || !target || !navMeshAgent->HasDestination())
		{
			StopCharge();
			return;
		}

		// 突進方向に対して、プレイヤーが後方へ抜けたら空振りとして終了する。
		Vector3 toTarget = target->transform.position - this->transform.position;
		toTarget.y = 0.0f;
		if (chargeDirection.Dot(toTarget) < 0.0f) StopCharge();
	};

	controller->AddAction("AttackChase", enterChase, updateChase, {}, stop);

}

void Deer::StopCharge()
{
	if (charging) cc->SetSlopeLimitDeg(normalSlopeLimit);
	charging = false;
	navMeshAgent->SetChargeMovement(false);
	controller->StopMovement();
	controller->SetBool("HasDestination", false);
	anim->SetFloat("speed", 0.0f);
}

void Deer::OnUpdate()
{
	if (deathCleanupPending)
	{
		Destroy();
		return;
	}

	Entity::OnUpdate();

	anim->SetBool("dead", IsDead());
	anim->SetFloat("speed", navMeshAgent->GetMoveAmount());
}

void Deer::OnLateUpdate()
{
	vmdl->UpdateTransform(transform.matrix);

	if (cc)
	{
		cc->ClearDebugRenderPosition();
	}
}

void Deer::OnDrawGUI()
{
	ImGui::TextDisabled("AI behavior and timings are edited in Resources/AI/Deer.lua.");
}

void Deer::OnTriggerEnter(
	PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (IsDead() || deathCleanupPending) return;
	if (!PhysicsComponent::IsLive(self) || !PhysicsComponent::IsLive(other)) return;
	if (!self->IsActive() || !other->IsActive()) return;
	// 自分がEnemyAtkじゃない場合　無視
	if (self->GetLayerId() != Layers::Get("EnemyAtk")) return;

	// プレイヤーが当たってきた
	if (other->GetLayerId() == Layers::Get("Player"))
	{
		Entity* player = dynamic_cast<Entity*>(other->GetOwner());
		if (!player || !player->CompareTag("Player")) return;

		anim->SetTrigger("Damage");
		StopCharge();

		DamageData damageData{
			.damage = 10.0f,
			.knockBackPower = 5.0f,
			.hitColliderSelf = self,
			.hitColliderOther = other,
			.hitPosition = point,
			.hitNormal = normal,
		};
		player->TakeDamage(damageData);
	}
}

void Deer::OnDamaged(const DamageData& damageData)
{
	TimeScaleController::Request(0.15f);
	CameraEffectController::Request(0.2f, 0.1f);

	StopCharge();
	if (damageData.hitColliderSelf)
	{
		Actor* attacker = dynamic_cast<Actor*>(damageData.hitColliderSelf->GetOwner());
		if (attacker) controller->LockOn(attacker);
	}

	// ダメージサウンド
	vmdl->GetRenderer()->PlaySoundSource("DAMAGED");
}

void Deer::OnDead(const DamageData& damageData)
{
	if (deathCleanupPending) return;

	SoundSystem::SpatialOptions deathSoundOptions;
	deathSoundOptions.minDistance = 15.0f;
	deathSoundOptions.maxDistance = 20.0f;
	SoundSystem::Instance().PlayTrack3DAt(
		SoundTrack::SE_DEER_DIE, transform.position, -1, deathSoundOptions);

	StopCharge();
	navMeshAgent->SetActive(false);
	controller->SetActive(false);

	// Destruction is deferred until the next actor update, but collision must disappear
	// on the exact frame life reaches zero.
	for (PhysicsComponent* collider : GetComponents<PhysicsComponent>())
	{
		if (collider) collider->SetActive(false);
	}
	if (cc) cc->ReleaseController();

	vmdl->GetRenderer()->PlayParticleEmitter("DEAD");
	vmdl->GetRenderer()->PlayParticleEmitter("DEAD2");

	deathCleanupPending = true;
}
