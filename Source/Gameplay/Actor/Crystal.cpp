#include "Gameplay/Actor/Crystal.h"

#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Rendering/Component/VMDL.h"

Crystal::Crystal(const Transform& transform) : Entity("Crystal", "Crystal", true, transform)
{
	this->transform.Update();

	float largestScale = std::max(fabsf(this->transform.scale.x), fabsf(this->transform.scale.y));
	largestScale = std::max(largestScale, fabsf(this->transform.scale.z));

	constexpr float baseScale = 0.5f;
	constexpr float lifePerScale = 87.0f;

	maxLife = ceilf((largestScale - baseScale) * lifePerScale);
	maxLife = std::clamp(maxLife, 1.0f, 100.0f);
	life = maxLife;

	vmdl = AddComponent<VMDL>("Resources/Model/Crystal");
}

void Crystal::OnUpdate()
{
	if (deathCleanupPending)
	{
		Destroy();
		return;
	}

	Entity::OnUpdate();
}

// 音
void Crystal::PlayBreakSound()
{
	float largestScale = std::max(fabsf(transform.scale.x), fabsf(transform.scale.y));
	largestScale = std::max(largestScale, fabsf(transform.scale.z));

	const bool isLarge = largestScale >= 1.0f;
	const char* sourceName = isLarge ? "BREAK_LARGE" : "BREAK_SMALL";
	vmdl->GetRenderer()->PlaySoundSource(sourceName, &transform.position);
}

// HitStop
void Crystal::OnDamaged(const DamageData& damageData)
{
	TimeScaleController::Request(0.06f);
	CameraEffectController::Request(0.1f, 0.06f);
}

void Crystal::OnDead(const DamageData& damageData)
{
	if (deathCleanupPending) return;

	life = 0.0f;
	PlayBreakSound();
	vmdl->GetRenderer()->PlayParticleEmitter("BREAK");

	deathCleanupPending = true;
}
