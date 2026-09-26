// VMDLParticleEmitterComponent.cpp
#include "Rendering/Component/VMDLParticleEmitterComponent.h"

#include "Rendering/Effect/EffectManager.h"

VMDLParticleEmitterComponent::VMDLParticleEmitterComponent(Object* owner, VMDLModel* model,
	const VMDLModel::VmdlParticleEmitter& settings, bool initiallyEmitting)
	: Component(owner), model(model), settings(settings)
{
	SetSettings(settings);
	SetEmitting(initiallyEmitting);
}

VMDLParticleEmitterComponent::~VMDLParticleEmitterComponent()
{
	Stop();
}

void VMDLParticleEmitterComponent::SetSettings(const VMDLModel::VmdlParticleEmitter& value)
{
	const bool restart = emitting;
	Stop();
	settings = value;
	effect.reset();
	if (!settings.effekseerData.empty())
		effect = std::make_unique<Effect>(
			settings.effekseerData.data(), settings.effekseerData.size());
	if (restart) Play();
}

void VMDLParticleEmitterComponent::SetEmitting(bool value)
{
	if (emitting == value) return;
	emitting = value;
	if (emitting) Play();
	else Stop();
}

void VMDLParticleEmitterComponent::Burst()
{
	Stop();
	Play();
}

void VMDLParticleEmitterComponent::Play()
{
	if (!effect || !effect->IsValid()) return;
	Matrix transform = settings.transform.ToMatrix();
	if (model && settings.nodeIndex >= 0 &&
		settings.nodeIndex < static_cast<int>(model->GetNodes().size()))
		transform = model->GetScaledAttachmentTransform(
			transform * model->GetNodes()[settings.nodeIndex].worldTransform);
	// 完成済みの行列をパーティクルに設定
	handle = effect->Play(Vector3::Zero);
	if (handle >= 0) effect->SetTransform(handle, transform);
}

void VMDLParticleEmitterComponent::Stop()
{
	if (handle < 0 || !effect) return;
	effect->Stop(handle);
	handle = -1;
}

void VMDLParticleEmitterComponent::OnLateUpdate()
{
	if (handle < 0 || !effect || !model || settings.nodeIndex < 0 ||
		settings.nodeIndex >= static_cast<int>(model->GetNodes().size())) return;
	if (!EffectManager::Instance().GetEffekseerManager()->Exists(handle))
	{
		handle = -1;
		return;
	}
	const Matrix transform = model->GetScaledAttachmentTransform(
		settings.transform.ToMatrix() * model->GetNodes()[settings.nodeIndex].worldTransform);
	effect->SetTransform(handle, transform);
}

void VMDLParticleEmitterComponent::OnDrawGUI()
{
	ImGui::Text("Effect: %s", settings.name.c_str());
	ImGui::Text("EFKPKG: %s", settings.effekseerFileName.c_str());
	bool active = emitting;
	if (ImGui::Checkbox("Playing", &active)) SetEmitting(active);
	if (ImGui::Button("Play Once")) Burst();
}
