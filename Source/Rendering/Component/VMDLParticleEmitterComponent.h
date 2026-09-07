#pragma once

#include <memory>
#include <deque>

#include "Core/Object/Component.h"
#include "Resource/VMDLModel.h"

class ParticleSystem;
class Texture;

// VMDLのボーンへ追従する、エディタ設定型の軽量パーティクルエミッタ。
class VMDLParticleEmitterComponent : public Component
{
public:
	VMDLParticleEmitterComponent(Object* owner, VMDLModel* model,
		const VMDLModel::VmdlParticleEmitter& settings, bool initiallyEmitting);
	~VMDLParticleEmitterComponent() override;

	void OnLateUpdate() override;
	void RenderParticles(const RenderContext& rc);
	void OnDrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MAGIC " Particle Emitter"; }

	void SetEmitting(bool value);
	bool IsEmitting() const { return emitting; }
	void Burst();
	const std::string& GetEmitterName() const { return settings.name; }
	float GetMaximumLifetime() const { return settings.lifetimeMax; }

private:
	void SpawnOne();
	void UpdateRibbon();
	void RenderRibbon(const RenderContext& rc);

	struct RibbonPoint
	{
		Vector3 root;
		Vector3 tip;
		Vector3 fullTip;
		float age = 0.0f;
	};

	VMDLModel* model = nullptr;
	VMDLModel::VmdlParticleEmitter settings;
	std::shared_ptr<Texture> texture;
	std::unique_ptr<ParticleSystem> particleSystem;
	bool emitting = false;
	bool burstPending = false;
	float emissionAccumulator = 0.0f;
	float ribbonSampleTimer = 0.0f;
	std::deque<RibbonPoint> ribbonPoints;
};
