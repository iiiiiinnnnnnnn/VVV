// EnemySmall.cpp

#include "EnemySmall.h"
#include "Animation/MultiLegFootIK.h"
#include "Gameplay/Scene/HitStop.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Application/Time/GameTime.h"

EnemySmall::EnemySmall(const Vector3& position, const Vector3& euler)
	: Entity("Deer(EnemySmall)", "Enemy", true, 100.0f, 100.0f)
{
	vmdl = AddComponent<VMDL>("Data/Model/Enemy/deer");
	anim = vmdl->GetAnimator();
	model = vmdl->GetSharedModel();
	vmdl->SetAutoUpdateTransform(false);
	auto footIK = vmdl->GetMultiLegFootIK();
	footIK->SetContactOffset(-0.34f);
	footIK->SetModelVisualOffsetY(-0.13f);
	anim->Load("Data/Animator/deer.animator");
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
	controller->SetGraphPath("Data/AI/enemysmall.json");
	if (!controller->Load(controller->GetGraphPath()))
		controller->CreateDefaultChaseGraph();
	controller->SetAgentRadius(1.5f);
	controller->SetTrackingTurnSpeed(1.0f);

	// FootIK
	auto ik = vmdl->GetMultiLegFootIK();
	ik->SetModelVisualOffsetY(-0.07f);
	ik->SetContactOffset(-0.044f);

	// Attacking
	const auto enterAttacking = [this](const EnemyAIFlow::State&)
	{
		attackPhaseTimer = attackWindupDuration;
		attackAimLocked = false;
		controller->StopMovement();
	};
	const auto updateAttacking = [this](const EnemyAIFlow::State&)
	{
		anim->SetBool("ready", true);

		if (!attackAimLocked)
		{
			controller->FaceTarget();
			attackPhaseTimer -= Game::Time::deltaTime;
			if (attackPhaseTimer > 0.0f) return;

			Actor* target = controller->GetTarget();
			if (!target) return;

			navMeshAgent->SetSpeed(attackMoveSpeed);
			navMeshAgent->MoveToPosition(target->transform.position);
			attackAimLocked = true;

			return;
		}

		if (navMeshAgent->HasDestination()) return;

		attackAimLocked = false;
		attackPhaseTimer = attackRecoveryDuration;
	};
	const auto exitAttacking = [this](const EnemyAIFlow::State&)
	{
		attackPhaseTimer = 0.0f;
		attackAimLocked = false;
		controller->StopMovement();
	};

	// Freedom
	const auto enterFreedom = [this](const EnemyAIFlow::State&)
	{
		navMeshAgent->Stop();
		freedomWaitTimer = 0.0f;
	};
	const auto updateFreedom = [this](const EnemyAIFlow::State&)
	{
		anim->SetBool("ready", false);

		if (navMeshAgent->HasDestination()) return;

		freedomWaitTimer -= Game::Time::deltaTime;
		if (freedomWaitTimer > 0.0f) return;

		navMeshAgent->SetSpeed(freedomMoveSpeed);
		if (navMeshAgent->MoveToRandomPosition(
			freedomMinDistance,
			freedomMaxDistance))
		{
			freedomWaitTimer = freedomWaitDuration;
			return;
		}

		freedomWaitTimer = 1.0f;
	};
	const auto exitFreedom = [this](const EnemyAIFlow::State&)
	{
		navMeshAgent->Stop();
		freedomWaitTimer = 0.0f;
	};
	controller->AddCallbackFunc(
		"Attacking",
		enterAttacking,
		updateAttacking,
		{},
		exitAttacking);
	controller->AddCallbackFunc(
		"Freedom",
		enterFreedom,
		updateFreedom,
		{},
		exitFreedom);
	controller->BindCallbacks();
}

void EnemySmall::OnUpdate()
{
	Entity::OnUpdate();

	anim->SetBool("dead", IsDead());
	anim->SetFloat("speed", navMeshAgent->GetMoveAmount());

	float turnAngle = navMeshAgent->GetTurnAngle();
	Vector3 turnDirection = Vector3::Zero;
	if (fabsf(turnAngle) <= eps && navMeshAgent->HasDestination())
	{
		turnDirection =
			navMeshAgent->GetDestination() -
			transform.position;
	}

	const EnemyAIFlow::State* aiState =
		controller->GetCurrentState();
	if (fabsf(turnAngle) <= eps &&
		turnDirection.LengthSquared() <= eps &&
		aiState &&
		aiState->callbackName == "Attacking")
	{
		Actor* target = controller->GetTarget();
		if (target)
		{
			turnDirection =
				target->transform.position -
				transform.position;
		}
	}

	turnDirection.y = 0.0f;
	if (fabsf(turnAngle) <= eps &&
		turnDirection.LengthSquared() > eps)
	{
		turnDirection.Normalize();
		turnAngle = atan2f(
			turnDirection.Dot(transform.right),
			turnDirection.Dot(transform.forward));
	}

	const bool turningRight =
		turnAngle > RAD(15.0f);
	const bool turningLeft =
		turnAngle < -RAD(15.0f);
	anim->SetBool("turnR", turningRight);
	anim->SetBool("turnL", turningLeft);
	navMeshAgent->SetMovementPaused(
		turningRight || turningLeft);

	//UpdateMovement();
	/*if (motor)
	{
		motor->SetExternalVelocity(knockBackVelocity);
	}*/
}

void EnemySmall::OnLateUpdate()
{
	model->UpdateTransform(transform.matrix);

	if (cc)
	{
		cc->ClearDebugRenderPosition();
	}
}

void EnemySmall::OnDrawGUI()
{
	ImGui::Checkbox("AttackAimLocked", &attackAimLocked);
	ImGui::DragFloat("FreedomWaitDuration", &freedomWaitDuration, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("FreedomMinDistance", &freedomMinDistance, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("FreedomMaxDistance", &freedomMaxDistance, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("FreedomMoveSpeed", &freedomMoveSpeed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("AttackMoveSpeed", &attackMoveSpeed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("AttackWindupDuration", &attackWindupDuration, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat("AttackRecoveryDuration", &attackRecoveryDuration, 0.1f, 0.0f, 10.0f);
}

void EnemySmall::OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	Entity* player = dynamic_cast<Entity*>(other->GetOwner());
	if (!player || !player->CompareTag("Player")) return;

	if (self->GetLayerId() != Layers::Get("EnemyAtk")) return;

	DamageData damageData{
		.damage = 10.0f,
		.knockBackPower = 5.0f,
		.hitColliderSelf = self,
		.hitColliderOther = other,
		.hitPosition = point,
		.hitNormal = normal,
	};
	player->TakeDamage(damageData);
	anim->SetTrigger("damaged");
}

void EnemySmall::OnDamaged(const DamageData& damageData)
{
	HitStop::Request(0.15f);
	CameraEffectController::Request(0.2f, 0.1f);
	navMeshAgent->Stop();
	controller->LockOn((Actor*)damageData.hitColliderSelf->GetOwner());
}

void EnemySmall::OnDead(const DamageData& damageData)
{
	navMeshAgent->Stop();
	controller->SetActive(false);
	Destroy();
}
