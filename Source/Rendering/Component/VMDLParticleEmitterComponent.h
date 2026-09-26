// VMDLParticleEmitterComponent.h
// VMDLParticleEmitterComponent.h
#pragma once

#include <memory>

#include "Core/Object/Component.h"
#include "Rendering/Effect/Effect.h"
#include "Resource/VMDLModel.h"

// VMDLに埋め込まれたEffekseerエフェクトをノードへ追従させる。
class VMDLParticleEmitterComponent : public Component
{
public:
	VMDLParticleEmitterComponent(Object* owner, VMDLModel* model,
		const VMDLModel::VmdlParticleEmitter& settings, bool initiallyEmitting);
	~VMDLParticleEmitterComponent() override;
	void OnLateUpdate() override;
	void RenderParticles(const RenderContext&) {}
	void OnDrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MAGIC " Effekseer Effect"; }
	void SetEmitting(bool value);
	bool IsEmitting() const { return emitting; }
	bool IsPlaying() const { return handle >= 0; }
	void Burst();
	void SetSettings(const VMDLModel::VmdlParticleEmitter& value);
	const std::string& GetEmitterName() const { return settings.name; }
	float GetMaximumLifetime() const { return effect && effect->IsValid() ? 1.0f : 0.0f; }

private:
	void Play();
	void Stop();
	VMDLModel* model = nullptr;
	VMDLModel::VmdlParticleEmitter settings;
	std::unique_ptr<Effect> effect;
	Effekseer::Handle handle = -1;
	bool emitting = false;
};
