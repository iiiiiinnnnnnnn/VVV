#include "EnemySmall.h"
#include "Animation/MultiLegFootIK.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Audio/SoundSystem.h"

EnemySmall::EnemySmall(const Vector3& position, const Vector3& euler)
	: Entity("Deer(EnemySmall)", "Enemy", true, 200.0f, 200.0f)
{
	vmdl = AddComponent<VMDL>("Resources/Model/Enemy/deer_ai_animated");
	anim = vmdl->GetAnimator();
	model = vmdl->GetSharedModel();
	vmdl->SetAutoUpdateTransform(false);
	auto footIK = vmdl->GetMultiLegFootIK();
	footIK->SetContactOffset(-0.864f);
	footIK->SetModelVisualOffsetY(-0.22f);
	anim->Load("Resources/Animator/deer.animator");
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

	cc->SetPosition(position);
	transform.SetAngle(euler);

	// NavMeshAgent
	navMeshAgent = AddComponent<NavMeshAgent>();
	navMeshAgent->SetStoppingDistance(0.8f);
	navMeshAgent->SetTurnSpeed(1.0f);

	// LookAt
	auto lookAt = AddComponent<LookAt>(model.get(), "spine", "neck");
	lookAt->SetFilterTags({"Player", "Enemy"});
	lookAt->SetLookDistance(8.0f);

	// EnemyAIFlow
	controller = AddComponent<EnemyAIFlow>();
	controller->SetGraphPath("Resources/AI/enemysmall.json");
	if (!controller->Load(controller->GetGraphPath()))
		controller->CreateDefaultChaseGraph();
	controller->SetAgentRadius(1.5f);
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
	ensureFloat("AttackOverDistance", 8.0f);

	const auto stop = [this](const EnemyAIFlow::State&)
	{
		controller->StopMovement();
	};
	const auto updateFreedom = [this](const EnemyAIFlow::State&)
	{
		anim->SetBool("ready", false);
	};
	const auto updateAttacking = [this](const EnemyAIFlow::State&)
	{
		anim->SetBool("ready", true);
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
	const auto enterChase = [this](const EnemyAIFlow::State&)
	{
		Actor* target = controller->GetTarget();
		if (!target) return;

		Vector3 over = target->transform.position - transform.position;
		over.y = 0.0f;
		if (over.LengthSquared() > eps) over.Normalize();
		const Vector3 destination = target->transform.position +
			over * controller->GetFloat("AttackOverDistance", 8.0f);
		navMeshAgent->SetSpeed(controller->GetFloat("AttackMoveSpeed", 6.0f));
		navMeshAgent->MoveToPosition(destination);
	};

	controller->AddCallbackFunc("Freedom", stop, updateFreedom, {}, stop);
	controller->AddCallbackFunc("FreedomWait", stop);
	controller->AddCallbackFunc("FreedomWander", enterWander);
	controller->AddCallbackFunc("Turn", {}, updateTurn, {}, exitTurn);
	controller->AddCallbackFunc("Attacking", stop, updateAttacking, {}, stop);
	controller->AddCallbackFunc("FaceTarget", {}, updateFaceTarget);
	controller->AddCallbackFunc("AttackChase", enterChase);
	controller->AddCallbackFunc("Stop", stop);
	controller->BindCallbacks();
}

void EnemySmall::OnUpdate()
{
	if (deathCleanupPending)
	{
		Destroy();
		return;
	}

	Entity::OnUpdate();

	anim->SetBool("dead", IsDead());
	anim->SetFloat("speed", navMeshAgent->GetMoveAmount());

	//UpdateMovement();
	/*if (motor)
	{
		motor->SetExternalVelocity(knockBackVelocity);
	}*/
}

void EnemySmall::OnLateUpdate()
{
	vmdl->UpdateTransform(transform.matrix);

	if (cc)
	{
		cc->ClearDebugRenderPosition();
	}
}

void EnemySmall::OnDrawGUI()
{
	ImGui::TextDisabled("AI timings and movement values are edited in the AI Flow graph.");
}

void EnemySmall::OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (self->GetLayerId() != Layers::Get("EnemyAtk")) return;
	if (anim->GetCurrentStateName() != "run") return;

	anim->SetTrigger("Damage");
	navMeshAgent->Stop();

	Entity* player = dynamic_cast<Entity*>(other->GetOwner());
	if (!player || !player->CompareTag("Player")) return;

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

void EnemySmall::OnDamaged(const DamageData& damageData)
{
	TimeScaleController::Request(0.15f);
	CameraEffectController::Request(0.2f, 0.1f);
	navMeshAgent->Stop();
	controller->LockOn((Actor*)damageData.hitColliderSelf->GetOwner());
}

void EnemySmall::OnDead(const DamageData& damageData)
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

	deathCleanupPending = true;
}
