// CrystalProp.h

#pragma once

#include <memory>
#include <vector>

#include "Actor.h"
#include "Model.h"
#include "ShaderParam.h"
#include "StageLoader.h"
#include "UserSettingsManager.h"

class MeshCollider;
class ParticleSystem;
class PhysicsComponent;
class Rigidbody;

class CrystalProp : public Actor
{
public:
	CrystalProp(const StageLoader::CrystalData& crystalData);
	~CrystalProp() override = default;

	void ApplyStageData(const StageLoader::CrystalData& crystalData);
	void SetBreakParticleSystem(ParticleSystem* particleSystem) { breakParticleSystem = particleSystem; }
	void Update() override;
	void Render(const RenderContext& rc) override;
	void OnCollisionEnter(
		PhysicsComponent* self,
		PhysicsComponent* other,
		const Vector3& point,
		const Vector3& normal) override;
	void OnTriggerEnter(
		PhysicsComponent* self,
		PhysicsComponent* other,
		const Vector3& point,
		const Vector3& normal) override;

private:
	struct Instance
	{
		Transform transform = {};
		std::shared_ptr<Model> model = nullptr;
		Rigidbody* rigidbody = nullptr;
		MeshCollider* meshCollider = nullptr;
		bool isBroken = false;
	};

	static Matrix MakeColliderMatrix(const Matrix& world, Vector3& scale);
	static Matrix MakeMatrix(const Transform& transform);
	Vector3 GetInstancePosition(const Instance& instance) const;
	void SetColliderActive(Instance& instance, bool active);
	void SyncCollider(Instance& instance, const Matrix& world);
	void Break(Instance& instance);
	void SpawnBreakParticles(const Vector3& position);
	Instance* FindInstance(PhysicsComponent* physicsComponent);
	bool IsBreakLayer(LayerId layerId) const;
	void TryBreak(PhysicsComponent* self, PhysicsComponent* other);

	std::vector<Instance> instances = {};
	ShaderParamListWithMaterialName shaderParams = {};
	ParticleSystem* breakParticleSystem = nullptr;
	std::string modelPath = "";
	Transform parentTransform = {};
};
