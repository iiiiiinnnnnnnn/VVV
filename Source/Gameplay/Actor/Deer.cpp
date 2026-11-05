#include "Application/SettingsAndDebug/ProfileScope.h"
#include "Deer.h"

#include <cmath>
#include "Animation/MultiLegFootIK.h"
#include "Gameplay/Component/ActorHitstopComponent.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Audio/SoundSystem.h"
#include "Crystal.h"
#include "CrystalPart.h"
#include "Gameplay/AI/EnemyAI.h"
#include "Animation/LookAt.h"
#include "Physics/Collider/CharacterController.h"
#include "Animation/Animator.h"
#include "Rendering/Component/VMDL.h"
#include "Rendering/Component/HitstopVibrationComponent.h"
#include "Gameplay/Scene/TimeScaleController.h"

Deer::Deer(const Transform& transform, const std::string& modelPath, bool inBossField)
	: Entity(inBossField ? "BossFieldDeer" : "Deer", "Enemy", true, transform, 200.0f, 200.0f),
	  modelPath(modelPath)
{}

Deer::~Deer()
{
	Uninit();
}

void Deer::OnAwake()
{
	vmdl = AddComponent<VMDL>(modelPath);
	vmdl->Awake();
	anim = AddComponent<Animator>(vmdl->GetSharedModel());
	vmdl->BuildFootIK(anim);
	model = vmdl->GetSharedModel();
	vmdl->SetAutoUpdateTransform(false);

	// メス設定(1/3)
	isFemare = Random::Range(0, 2) == 0;
	if (isFemare) vmdl->ApplyMorph("HIDE_CRYSTAL");
	if (isFemare)
	{
		vmdl->GetSoundSource("DEAD")->pitchMax = 1.1f;
		vmdl->GetSoundSource("DEAD")->pitchMin = 1.2f;
	}

	auto footIK = vmdl->GetMultiLegFootIK();
	footIK->SetContactOffset(-0.864f);
	footIK->SetModelVisualOffsetY(-0.22f);
	anim->Load("Deer");
	anim->SetRootMotion("armature", false);

	// キャラクターコントローラー
	cc = AddComponent<CharacterController>(Layers::Get("Enemy"), 0.39f, 1.23f);
	cc->SetStepOffset(0.15f);
	cc->SetSlopeLimitDeg(70.0f);
	cc->SetContactOffset(0.1f);

	// HitstopVibrationComponent
	hitstopVibration = AddComponent<HitstopVibrationComponent>(model);
	AddComponent<ActorHitstopComponent>(model);

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
	controller = AddComponent<EnemyAI>("Deer.lua");

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

	// 戦闘共通では移動の後始末だけを行い、構えは突進準備の処理で管理する。
	controller->AddAction("Attacking", stop, {}, {}, stop);

	// 徘徊：ランダムな目的地へ歩く。
	const auto enterWander = [this]()
	{
		navMeshAgent->SetSpeed(controller->GetFloat("FreedomMoveSpeed", 1.0f));
		navMeshAgent->MoveToRandomPosition(
			controller->GetFloat("FreedomMinDistance", 3.0f),
			controller->GetFloat("FreedomMaxDistance", 10.0f));
	};

	controller->AddAction("FreedomWander", enterWander);

	const auto updateTurnAnimation = [this](float angle)
	{
		const bool turning = std::abs(angle) > 1.0f;
		anim->SetBool("turnR", turning && angle > 0.0f);
		anim->SetBool("turnL", turning && angle < 0.0f);
		return turning;
	};

	// 徘徊中は移動先へ、戦闘中は相手へ向き直る角度でアニメーションを選ぶ。
	const auto updateTurn = [this, updateTurnAnimation]()
	{
		const float angle = controller->GetFloat(navMeshAgent->HasDestination() ? "TurnAngle" : "TargetTurnAngle");
		updateTurnAnimation(angle);
		anim->SetBool("ready", false);
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

	// 突進準備の間だけ構えを有効にし、終了時は突進・旋回・休憩のどれへ進んでも解除する。
	const auto enterPreparation = [this]()
	{
		anim->SetBool("turnR", false);
		anim->SetBool("turnL", false);
		anim->SetBool("ready", true);
	};
	const auto updateFaceTarget = [this, enterPreparation]()
	{
		enterPreparation();
		controller->FaceTarget();
	};
	const auto exitPreparation = [this, exitTurn]()
	{
		anim->SetBool("ready", false);
		exitTurn();
	};

	controller->AddAction("FaceTarget", enterPreparation, updateFaceTarget, {}, exitPreparation);

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
	anim->SetBool("ready", false);
}

void Deer::OnUpdate()
{
	PROFILE_FUNCTION();
	if (deathCleanupPending)
	{
		if (GetComponent<ActorHitstopComponent>()->IsStopping()) return;
		// deer消える直前にクリスタル
		CrystalPart::Spawn(transform.position + Vector3(0.0f, 2.0f, 0.0f),
			{0.0f, 3.0f, 0.0f}, 3, 0.4f, 4.0f, 0.5f, this);
		Destroy();
		return;
	}

	Entity::OnUpdate();

	anim->SetBool("dead", IsDead());
	anim->SetFloat("speed", navMeshAgent->GetMoveAmount());
}

void Deer::OnLateUpdate()
{
	PROFILE_FUNCTION();
	vmdl->UpdateTransform(transform.GetMatrix());

	if (cc)
	{
		cc->ClearDebugRenderPosition();
	}
}

void Deer::OnDrawGUI()
{
	ImGui::TextUnformatted((const char*)u8"AIパス：Resources/AI/Deer.lua");
}

void Deer::OnCollisionEnter(
	PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (IsDead() || deathCleanupPending) return;
	if (!PhysicsComponent::IsLive(self) || !PhysicsComponent::IsLive(other)) return;
	if (!self->IsActive() || !other->IsActive()) return;

	BreakCrystalOnContact(self, other, point, normal);
}

void Deer::BreakCrystalOnContact(
	PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (!anim || anim->GetCurrentStateName() != "run") return;

	auto* crystal = dynamic_cast<Crystal*>(other->GetOwner());
	if (!crystal || crystal->IsDead()) return;

	crystal->TakeDamage({
		.damage = crystal->GetLife(),
		.isForceDamage = true,
		.attackerCollider = self,
		.targetCollider = other,
		.hitPosition = point,
		.hitNormal = normal,
	});
}

void Deer::OnTriggerEnter(
	PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (IsDead() || deathCleanupPending) return;
	if (!PhysicsComponent::IsLive(self) || !PhysicsComponent::IsLive(other)) return;
	if (!self->IsActive() || !other->IsActive()) return;

	// クリスタルが当たってきた
	BreakCrystalOnContact(self, other, point, normal);

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
			.knockBackPower = 7.0f,
			.attackerCollider = self,
			.targetCollider = other,
			.hitPosition = point,
			.hitNormal = normal,
		};
		player->TakeDamage(damageData);
	}
}

void Deer::OnDamaged(const DamageData& damageData)
{
	if (damageData.attackerCollider && damageData.attackerCollider->GetLayerId() == Layers::Get("PlayerAtk"))
	{
		// プレイヤーからの攻撃だけhitstopとか再生
		GetComponent<ActorHitstopComponent>()->Request(0.15f);
		CameraEffectController::Request(0.2f, 0.1f);
		hitstopVibration->HitStop(0.12f, 0.1f);
	}

	StopCharge();
	if (damageData.attackerCollider)
	{
		Actor* attacker = dynamic_cast<Actor*>(damageData.attackerCollider->GetOwner());
		if (attacker) controller->LockOn(attacker);
	}

	// ダメージサウンド
	vmdl->GetRenderer()->PlaySoundSource("DAMAGED");
}

void Deer::OnDead(const DamageData& damageData)
{
	if (deathCleanupPending) return;
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

	vmdl->GetRenderer()->PlayParticleEmitter("DEAD", true);
	vmdl->GetRenderer()->PlayParticleEmitter("DEAD2", true);
	vmdl->GetRenderer()->PlaySoundSource("DEAD");

	deathCleanupPending = true;
}
