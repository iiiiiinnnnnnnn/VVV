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
	void OnDrawGUI() override;
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
	ShaderParamList MakePBRParams() const;
	void ApplyMaterialParams();

	std::vector<Instance> instances = {};
	ShaderParamListWithMaterialName shaderParams = {};
	ParticleSystem* breakParticleSystem = nullptr;
	std::string modelPath = "";
	Transform parentTransform = {};
	Color color = Color(0.12f, 0.62f, 1.0f, 1.0f);
	Color emission = Color(0.02f, 0.55f, 1.0f, 0.35f);
	Color fresnelColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
	float fresnelPower = 3.3f;
	float fresnelStrength = 1.78f;
	float metallic = 0.35f;
	float roughness = 0.08f;
	float occlusion = 1.0f;
	float occlusionStrength = 1.0f;
	float shadowStrength = 0.25f;
	float colliderScale = 1.0f;
	bool isFlatShading = false;
};







