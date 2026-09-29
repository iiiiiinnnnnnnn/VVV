#pragma once

#include <memory>
#include <vector>

#include "Core/Object/Component.h"
#include "Rendering/Effect/Effect.h"
#include "Resource/VMDLModel.h"

class VMDLParticleEmitterComponent : public Component
{
public:
	VMDLParticleEmitterComponent(Object* owner, VMDLModel* model,
		const VMDLModel::VmdlParticleEmitter& settings, bool initiallyEmitting);
	~VMDLParticleEmitterComponent() override;
	void OnLateUpdate() override;
	void RenderParticles(const RenderContext& rc);
	void OnDrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MAGIC " Effekseer Effect"; }
	void SetEmitting(bool value);
	bool IsEmitting() const { return emitting; }
	bool IsPlaying() const { return !playbacks.empty(); }
	void Play();
	void SetSettings(const VMDLModel::VmdlParticleEmitter& value);
	const std::string& GetEmitterName() const { return settings.name; }
	bool IsValid() const { return effect && effect->IsValid(); }

private:
	struct Playback
	{
		Effekseer::Handle handle = -1;
		Matrix initialTransform = Matrix::Identity;
	};

	Matrix GetWorldTransform() const;
	void Stop();
	int GetUpdateOrder() const override { return 10; }
	VMDLModel* model = nullptr;
	VMDLModel::VmdlParticleEmitter settings;
	std::shared_ptr<Effect> effect;
	std::vector<Playback> playbacks;
	bool emitting = false;
};
