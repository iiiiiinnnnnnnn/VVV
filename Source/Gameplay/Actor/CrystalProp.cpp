#include "Gameplay/Actor/CrystalProp.h"

#include "Gameplay/Scene/CameraEffectController.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Scene/HitStop.h"
#include "Physics/Collider/MeshCollider.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Rendering/Component/DamageHoleComponent.h"
#include "Rendering/Effect/ParticleSystem.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Resource/ResourceManager.h"
#include "Physics/RigidBody/Rigidbody.h"

CrystalProp::CrystalProp(StageLoader::PropData& propData)
	: Entity(propData.name, propData.tag, true)
{
	transform = propData.transform;
	transform.Update();

	float largestScale = (std::max)(fabsf(transform.scale.x), fabsf(transform.scale.y));
	largestScale = (std::max)(largestScale, fabsf(transform.scale.z));
	constexpr float baseScale = 0.5f;
	constexpr float lifePerScale = 87.0f;
	maxLife = ceilf((largestScale - baseScale) * lifePerScale);
	maxLife = (std::clamp)(maxLife, 1.0f, 100.0f);
	life = maxLife;

	if (!propData.model) propData.model = ResourceManager::Instance().LoadModel(propData.modelPath);
	model = propData.model;
	modelRenderer = AddComponent<VMDLModelComponent>(model, ModelShaderId::VMat);
	damageHoleComponent = AddComponent<DamageHoleComponent>(modelRenderer, 0.5f, 0.5f, 0.5f, 0.1f);
	rigidbody = AddComponent<RigidbodyStatic>();
	meshCollider = AddComponent<MeshCollider>(Layers::Get("Prop"), rigidbody, model, true);
	meshCollider->SetTrigger(true);
}

void CrystalProp::ApplyStageData(StageLoader::PropData& propData)
{
	const bool scaleChanged =
		(transform.scale - propData.transform.scale).LengthSquared() > 0.000001f;
	SetName(propData.name);
	SetTag(propData.tag);
	transform = propData.transform;
	transform.Update();
	if (modelRenderer) modelRenderer->UpdateModelTransform(transform.matrix);
	if (rigidbody)
	{
		rigidbody->SetPosition(transform.position);
		rigidbody->SetRotation(transform.rotation);
	}
	if (scaleChanged && meshCollider) meshCollider->UpdateShape();
}

void CrystalProp::OnTriggerEnter(
	PhysicsComponent*, PhysicsComponent* other, const Vector3&, const Vector3&)
{
	if (!other) return;
	Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());
	if (!otherActor || !otherActor->CompareTag("Enemy")) return;
	Break();
}

void CrystalProp::Break()
{
	if (IsPendingDestroy()) return;
	life = 0.0f;
	SpawnBreakParticles();
	Destroy();
	if (destroyedCallback) destroyedCallback(this);
}

void CrystalProp::SpawnBreakParticles()
{
	if (!breakParticleSystem) return;

	float largestScale = (std::max)(fabsf(transform.scale.x), fabsf(transform.scale.y));
	largestScale = (std::max)(largestScale, fabsf(transform.scale.z));

	int particleCount = static_cast<int>(largestScale * 32.0f * 10.0f);
	for (int i = 0; i < particleCount; ++i)
	{
		Vector3 p = transform.position;
		p.x += Random::Range(-0.6f, 0.6f);
		p.y += Random::Range(+0.0f, 0.0f);
		p.z += Random::Range(-0.6f, 0.6f);

		Vector3 v;
		v.x = Random::Range(-0.75f, 0.75f);
		v.y = Random::Range(+2.45f, 3.05f);
		v.z = Random::Range(-0.75f, 0.75f);

		breakParticleSystem->Set(7, 5.2f, p, v, Vector3(0.0f, -5.0f, 0.0f), Vector2(0.2f, 0.2f),
			false, 24.0f, Color(0.35f, 0.9f, 1.0f, 1.0f));
	}
}

void CrystalProp::OnDamaged(const DamageData& damageData)
{
	HitStop::Request(0.06f);
	CameraEffectController::Request(0.1f, 0.06f);

	if (damageData.hitPosition.has_value())
		damageHoleComponent->AddDamageHoleFromPosition(
			damageData.hitPosition.value(), damageData.hitNormal.value_or(Vector3::Zero));
}

void CrystalProp::OnDead(const DamageData& damageData)
{
	Break();
}

void CrystalProp::Update()
{
	Actor::Update();
	rigidbody->SetPosition(transform.position);
	rigidbody->SetRotation(transform.rotation);
}
