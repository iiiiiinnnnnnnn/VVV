// EnemySmall.cpp

#include "EnemySmall.h"
#include "Animation/MultiLegFootIK.h"
#include "Gameplay/Scene/HitStop.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Application/Time/GameTime.h"

EnemySmall::EnemySmall(const Vector3& position)
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

	motor = AddComponent<CharacterMotorComponent>(anim, cc);
	motor->SetRootMotionNode("armature");
	motor->SetUseRootMotion(false);

	cc->SetPosition(position);

	// NavMeshAgent
	navMeshAgent = AddComponent<NavMeshAgent>();
	navMeshAgent->SetSpeed(0.1f);

	// LookAt
	auto lookAt = AddComponent<LookAt>(model.get(), "spine", "neck");
	lookAt->SetFilterTags({"Player", "Enemy"});
	lookAt->SetLookDistance(10.0f);

	// EnemyAIFlow
	controller = AddComponent<EnemyAIFlow>();
	controller->SetGraphPath("Data/AI/enemysmall.json");
	if (!controller->Load(controller->GetGraphPath()))
		controller->CreateDefaultChaseGraph();
	controller->SetAgentRadius(1.5f);

	const auto AI_Attacking = [this](const EnemyAIFlow::State&)
	{
		controller->MoveToTarget(attackMoveSpeed);
	};
	const auto exitAttacking = [this](const EnemyAIFlow::State&)
	{
		controller->StopMovement();
	};

	const auto enterFreedom = [this](const EnemyAIFlow::State&)
	{
		navMeshAgent->Stop();
		freedomWaitTimer = 0.0f;
	};
	const auto updateFreedom = [this](const EnemyAIFlow::State&)
	{
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
		{},
		AI_Attacking,
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

	//UpdateMovement();
	if (motor)
	{
		motor->SetExternalVelocity(knockBackVelocity);
	}
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
	ImGui::DragFloat("Freedom Wait Duration", &freedomWaitDuration, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Freedom Min Distance", &freedomMinDistance, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Freedom Max Distance", &freedomMaxDistance, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Freedom Move Speed", &freedomMoveSpeed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Attack Move Speed", &attackMoveSpeed, 0.1f, 0.0f, 30.0f);
}

void EnemySmall::OnCollisionEnter(
	PhysicsComponent* self,
	PhysicsComponent* other,
	const Vector3& point,
	const Vector3& normal)
{
	Entity* player = dynamic_cast<Entity*>(other->GetOwner());
	if (!player || !player->CompareTag("Player")) return;

	Vector3 pushDirection = player->transform.position - transform.position;
	pushDirection.y = 0.0f;
	if (pushDirection.LengthSquared() <= eps) return;

	pushDirection.Normalize();
	player->AddKnockBack(pushDirection * 3.0f);
}

void EnemySmall::OnDamaged(const DamageData& damageData)
{
	HitStop::Request(0.15f);
	CameraEffectController::Request(0.2f, 0.1f);
}
