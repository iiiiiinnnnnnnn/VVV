#include "Gameplay/Actor/Crystal.h"

#include "Gameplay/Actor/CrystalPart.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Component/ActorHitstopComponent.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Rendering/Component/VMDL.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Resource/ResourceManager.h"
#include "Rendering/Component/HitstopVibrationComponent.h"
#include <algorithm>
#include <cmath>

Crystal::Crystal(const Transform& transform, const std::string& modelPath)
	: Entity("Crystal", "Crystal", true, transform), modelPath(modelPath)
{}

Crystal::~Crystal()
{
	Uninit();
}

void Crystal::OnAwake()
{
	loadedModel = ResourceManager::Instance().LoadModel(modelPath, true);
	vmdl = AddComponent<VMDL>(loadedModel);
	vmdl->Awake();
	vmdl->UpdateTransform(transform.GetMatrix());

	// HitStop振動用コンポーネント
	hitstopVibration = AddComponent<HitstopVibrationComponent>(loadedModel);
	AddComponent<ActorHitstopComponent>(loadedModel);

	float largestScale = std::max(fabsf(this->transform.scale.x), fabsf(this->transform.scale.y));
	largestScale = std::max(largestScale, fabsf(this->transform.scale.z));

	constexpr float baseScale = 0.5f;
	constexpr float lifePerScale = 87.0f;

	maxLife = ceilf((largestScale - baseScale) * lifePerScale);
	maxLife = std::clamp(maxLife, 1.0f, 100.0f);
	life = maxLife;

	if (!loadedModel->GetLocalBounds(crystalCenter, crystalSize)) crystalSize = Vector3::Zero;
}

void Crystal::OnUpdate()
{
	if (deathCleanupPending)
	{
		if (GetComponent<ActorHitstopComponent>()->IsStopping()) return;
		CrystalPart::Spawn(transform, crystalCenter, crystalSize, {0.0f, 3.0f, 0.0f}, 4.0f, 0.6f, this);
		Destroy();
		return;
	}

	Entity::OnUpdate();
}

void Crystal::OnDamaged(const DamageData& damageData)
{
	vmdl->GetRenderer()->PlaySoundSource("ATTACK", &transform.position);
	if (damageData.attackerCollider && damageData.attackerCollider->GetOwner()->CompareTag("Player"))
	{
		// プレイヤーだけhitstop
		GetComponent<ActorHitstopComponent>()->Request(0.07f);
		CameraEffectController::Request(0.1f, 0.06f);
		hitstopVibration->HitStop(0.1f, 0.1f);
	}
}

void Crystal::OnDead(const DamageData& damageData)
{
	if (deathCleanupPending) return;

	life = 0.0f;
	deathCleanupPending = true;
	for (PhysicsComponent* collider : GetComponents<PhysicsComponent>())
		if (collider) collider->SetActive(false);
	PlayBreakSound();
	PlayBreakParticle();
}

bool Crystal::PlayBreakSound()
{
	float largestScale = std::max(fabsf(transform.scale.x), fabsf(transform.scale.y));
	largestScale = std::max(largestScale, fabsf(transform.scale.z));

	const bool isLarge = largestScale >= 1.0f;
	const char* sourceName = isLarge ? "BREAK_LARGE" : "BREAK_SMALL";
	if (vmdl->GetRenderer()->PlaySoundSource(sourceName, &transform.position)) return true;
	return vmdl->GetRenderer()->PlaySoundSource("BREAK", &transform.position);
}

bool Crystal::PlayBreakParticle()
{
	return vmdl->GetRenderer()->PlayParticleEmitter("BREAK");
}
