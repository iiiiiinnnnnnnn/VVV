// EnemySmall.cpp

#include "EnemySmall.h"
#include "Animation/MultiLegFootIK.h"

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
	cc->SetStepOffset(1.2f);
	cc->SetSlopeLimitDeg(70.0f);
	cc->SetContactOffset(0.1f);

	motor = AddComponent<CharacterMotorComponent>(anim, cc);
	motor->SetRootMotionNode("armature");

	// SetFootPositionとSetPositionは両方呼ばない
	cc->SetFootPosition({ 0.0f, 5.0f, 10.0f });

	// LookAt
	lookAt = AddComponent<LookAt>(
		model.get(), "spine", "neck");
	lookAt->SetActive(false);

	cc->SetPosition(position);
}

void EnemySmall::OnUpdate()
{
	Entity::OnUpdate();

	UpdateLookIn();
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
}

void EnemySmall::UpdateLookIn()
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
		if (!actor->CompareTag("Player")) continue;

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
