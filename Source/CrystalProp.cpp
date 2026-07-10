// CrystalProp.cpp

#include "CrystalProp.h"

#include "CameraEffectController.h"
#include "Graphics.h"
#include "HitStop.h"
#include "MeshCollider.h"
#include "ModelRenderer.h"
#include "ParticleSystem.h"
#include "PhysicsComponent.h"
#include "ResourceManager.h"
#include "Rigidbody.h"

CrystalProp::CrystalProp(const StageLoader::CrystalData& crystalData)
	: Actor("CrystalProp", "CrystalProp", true)
{
	ApplyStageData(crystalData);
}

Matrix CrystalProp::MakeColliderMatrix(const Matrix& world, Vector3& scale)
{
	Quaternion rotation;
	Vector3 position;
	Matrix copy = world;
	copy.Decompose(scale, rotation, position);
	return Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
}

Matrix CrystalProp::MakeMatrix(const Transform& transform)
{
	return Matrix::CreateScale(transform.scale) *
		Matrix::CreateFromQuaternion(transform.rotation) *
		Matrix::CreateTranslation(transform.position);
}

Vector3 CrystalProp::GetInstancePosition(const Instance& instance) const
{
	Matrix world = MakeMatrix(instance.transform) * parentTransform.matrix;
	Vector3 scale;
	Quaternion rotation;
	Vector3 position;
	world.Decompose(scale, rotation, position);
	return position;
}

void CrystalProp::SetColliderActive(Instance& instance, bool active)
{
	if (!instance.rigidbody) return;

	if (active)
	{
		instance.rigidbody->SetSceneEnabled(true);
		if (instance.meshCollider) instance.meshCollider->SetCollisionEnabled(true);
		return;
	}

	if (instance.meshCollider) instance.meshCollider->SetCollisionEnabled(false);
	instance.rigidbody->SetSceneEnabled(false);
}

void CrystalProp::SyncCollider(Instance& instance, const Matrix& world)
{
	if (!instance.model) return;

	Vector3 scale;
	Matrix colliderMatrix = MakeColliderMatrix(world, scale);

	if (!instance.rigidbody)
	{
		instance.rigidbody = AddComponent<RigidbodyStatic>(colliderMatrix);
		instance.meshCollider = AddComponent<MeshCollider>(Layers::Get("Prop"), instance.rigidbody, instance.model.get(), scale);
	}
	else
	{
		instance.rigidbody->GetRigidActor()->setGlobalPose(Conv::ToPxTransform(colliderMatrix));
		if (instance.meshCollider) instance.meshCollider->SetLocalScale(scale);
	}

	SetColliderActive(instance, true);
}

CrystalProp::Instance* CrystalProp::FindInstance(PhysicsComponent* physicsComponent)
{
	if (!physicsComponent) return nullptr;

	for (Instance& instance : instances)
	{
		if (instance.meshCollider == physicsComponent) return &instance;
	}

	return nullptr;
}

void CrystalProp::SpawnBreakParticles(const Vector3& position)
{
	if (!breakParticleSystem) return;

	const int particleCount = 32;
	for (int i = 0; i < particleCount; ++i)
	{
		Vector3 p = position;
		p.x += Random::Range(-0.6f, 0.6f);
		p.y += Random::Range(+0.7f, 1.0f);
		p.z += Random::Range(-0.6f, 0.6f);

		Vector3 v;
		v.x = Random::Range(-1.75f, 1.75f);
		v.y = Random::Range(-0.45f, 1.05f);
		v.z = Random::Range(-1.75f, 1.75f);

		Vector3 f(0.0f, -5.0f, 0.0f);
		Vector2 size(1.0f, 1.0f);
		Color color(0.35f, 0.9f, 1.0f, 1.0f);
		breakParticleSystem->Set(7, 1.2f, p, v, f, size, false, 24.0f, color);
	}
}

void CrystalProp::Break(Instance& instance)
{
	if (instance.isBroken) return;

	Vector3 position = GetInstancePosition(instance);
	instance.isBroken = true;
	SetColliderActive(instance, false);
	SpawnBreakParticles(position);
}

void CrystalProp::ApplyStageData(const StageLoader::CrystalData& crystalData)
{
	parentTransform = crystalData.parentTransform;

	if (modelPath != crystalData.modelPath)
	{
		for (Instance& instance : instances)
		{
			SetColliderActive(instance, false);
		}

		modelPath = crystalData.modelPath;
		instances.clear();
	}

	for (size_t i = crystalData.transforms.size(); i < instances.size(); ++i)
	{
		SetColliderActive(instances[i], false);
	}

	instances.resize(crystalData.transforms.size());

	for (size_t i = 0; i < crystalData.transforms.size(); ++i)
	{
		Instance& instance = instances[i];
		instance.transform = crystalData.transforms[i];

		if (!instance.model)
		{
			instance.model = ResourceManager::Instance().LoadModel(modelPath);
		}

		ModelRenderer::SetShaderParamForAllMaterials(
			instance.model.get(),
			crystalData.MakePBRParams(),
			shaderParams);
	}
}

bool CrystalProp::IsBreakLayer(LayerId layerId) const
{
	return layerId == Layers::Get("Enemy") ||
		layerId == Layers::Get("PlayerAtk") ||
		layerId == Layers::Get("PlayerAttack") ||
		layerId == Layers::Get("EnemyAtk") ||
		layerId == Layers::Get("EnemyAttack");
}

void CrystalProp::TryBreak(PhysicsComponent* self, PhysicsComponent* other)
{
	if (!other) return;
	if (!IsBreakLayer(other->GetLayerId())) return;

	HitStop::Request(0.06f);
	CameraEffectController::Request(0.1f, 0.06f);

	if (Instance* instance = FindInstance(self))
	{
		Break(*instance);
	}
}

void CrystalProp::Update()
{
	Actor::Update();
	parentTransform.Update();

	for (Instance& instance : instances)
	{
		if (instance.isBroken)
		{
			SetColliderActive(instance, false);
			continue;
		}

		instance.transform.Update();
		Matrix world = instance.transform.matrix * parentTransform.matrix;
		if (instance.model) instance.model->UpdateTransform(world);
		SyncCollider(instance, world);
	}
}

void CrystalProp::Render(const RenderContext& rc)
{
	for (const Instance& instance : instances)
	{
		if (instance.isBroken) continue;
		if (!instance.model) continue;
		Game::Graphics::Instance().GetModelRenderer()->Draw(
			ModelShaderId::PBR, instance.model, shaderParams);
	}
}

void CrystalProp::OnCollisionEnter(
	PhysicsComponent* self,
	PhysicsComponent* other,
	const Vector3& point,
	const Vector3& normal)
{
	TryBreak(self, other);
}

void CrystalProp::OnTriggerEnter(
	PhysicsComponent* self,
	PhysicsComponent* other,
	const Vector3& point,
	const Vector3& normal)
{
	TryBreak(self, other);
}
