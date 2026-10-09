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

	// EnemyAIFlow
	ConfigureAI();
}

void Deer::ConfigureAI()
{
	controller = AddComponent<EnemyAIFlow>();
	controller->SetGraphPath("Resources/AI/Deer.json");
	if (!controller->Load(controller->GetGraphPath()))
		controller->CreateDefaultChaseGraph();
	controller->SetTrackingTurnSpeed(1.0f);

	// FootIK
	auto ik = vmdl->GetMultiLegFootIK();
	ik->SetModelVisualOffsetY(-0.07f);
	ik->SetContactOffset(-0.044f);

	const auto ensureFloat = [this](const std::string& name, float value)
	{
		for (const AIFlow::Parameter& parameter : controller->GetParameters())
			if (parameter.name == name) return;
		controller->SetFloat(name, value);
	};
	ensureFloat("FreedomMinDistance", 3.0f);
	ensureFloat("FreedomMaxDistance", 10.0f);
	ensureFloat("FreedomMoveSpeed", 1.0f);
	ensureFloat("AttackMoveSpeed", 6.0f);
	ensureFloat("AttackOverDistance", 16.0f);

	const auto stop = [this](const EnemyAIFlow::State&)
	{
		controller->StopMovement();
	};
	const auto updateFreedom = [this](const EnemyAIFlow::State&)
	{
		anim->SetBool("ready", false);
	};
	const auto canCharge = [this]()
	{
		Actor* target = controller->GetTarget();
		if (!target) return false;
		NavMeshActor* navMesh = NavMeshActor::GetActive();
		if (!navMesh) return true;
		// NavMeshの外側や境界付近にいる場合はチャージしない
		const bool deerOutside = navMesh->IsOutsideOrNearBoundary(this->transform.position, 0.0f);
		const bool targetOutside = navMesh->IsOutsideOrNearBoundary(target->transform.position, 0.0f);
		return !deerOutside && !targetOutside;
	};
	const auto updateAttacking = [this, canCharge](const EnemyAIFlow::State&)
	{
		anim->SetBool("ready", canCharge());
	};
	const auto enterWander = [this](const EnemyAIFlow::State&)
	{
		navMeshAgent->SetSpeed(controller->GetFloat("FreedomMoveSpeed", 1.0f));
		navMeshAgent->MoveToRandomPosition(
			controller->GetFloat("FreedomMinDistance", 3.0f),
			controller->GetFloat("FreedomMaxDistance", 10.0f));
	};
	const auto updateTurn = [this](const EnemyAIFlow::State&)
	{
		const float angle = controller->GetFloat("TurnAngle");
		anim->SetBool("turnR", angle > 0.0f);
		anim->SetBool("turnL", angle < 0.0f);
		navMeshAgent->SetMovementPaused(true);
		if (!navMeshAgent->HasDestination()) controller->FaceTarget();
	};
	const auto exitTurn = [this](const EnemyAIFlow::State&)
	{
		anim->SetBool("turnR", false);
		anim->SetBool("turnL", false);
		navMeshAgent->SetMovementPaused(false);
	};
	const auto updateFaceTarget = [this](const EnemyAIFlow::State&)
	{
		controller->FaceTarget();
	};
	const auto enterChase = [this, canCharge](const EnemyAIFlow::State&)
	{
		if (!canCharge())
		{
			controller->StopMovement();
			controller->SetBool("HasDestination", false, true);
			anim->SetBool("ready", false);
			anim->SetFloat("speed", 0.0f);
			return;
		}
		Actor* target = controller->GetTarget();
		if (!target) return;

		Vector3 over = target->transform.position - this->transform.position;
		over.y = 0.0f;
		if (over.LengthSquared() > eps) over.Normalize();
		const float overDistance = controller->GetFloat("AttackOverDistance", 16.0f);
		const float moveSpeed = controller->GetFloat("AttackMoveSpeed", 6.0f);
		const Vector3 chargeOffset = over * overDistance;
		const Vector3 destination = target->transform.position + chargeOffset;
		navMeshAgent->SetSpeed(moveSpeed);
		navMeshAgent->MoveToPosition(destination);
	};
	const auto updateChase = [this, canCharge](const EnemyAIFlow::State&)
	{
		if (canCharge()) return;
		controller->StopMovement();
		controller->SetBool("HasDestination", false, true);
		anim->SetBool("ready", false);
		anim->SetFloat("speed", 0.0f);
	};

	controller->AddCallbackFunc("Freedom", stop, updateFreedom, {}, stop);
	controller->AddCallbackFunc("FreedomWait", stop);
	controller->AddCallbackFunc("FreedomWander", enterWander);
	controller->AddCallbackFunc("Turn", {}, updateTurn, {}, exitTurn);
	controller->AddCallbackFunc("Attacking", stop, updateAttacking, {}, stop);
	controller->AddCallbackFunc("FaceTarget", {}, updateFaceTarget);
	controller->AddCallbackFunc("AttackChase", enterChase, updateChase);
	controller->AddCallbackFunc("Stop", stop);
	controller->BindCallbacks();
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
	ImGui::TextDisabled("AI timings and movement values are edited in the AI Flow graph.");
}

void Deer::OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (!self || !other || !self->IsActive() || !other->IsActive()) return;
	if (self->GetLayerId() != Layers::Get("EnemyAtk")) return;
	if (anim->GetCurrentStateName() != "run") return;

	Entity* player = dynamic_cast<Entity*>(other->GetOwner());
	if (!player || !player->CompareTag("Player")) return;
	if (other->GetLayerId() != Layers::Get("Player")) return;

	anim->SetTrigger("Damage");
	navMeshAgent->Stop();

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

void Deer::OnDamaged(const DamageData& damageData)
{
	TimeScaleController::Request(0.15f);
	CameraEffectController::Request(0.2f, 0.1f);

	navMeshAgent->Stop();
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

	navMeshAgent->Stop();
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
