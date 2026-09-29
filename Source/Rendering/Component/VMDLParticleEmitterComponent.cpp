#include "Rendering/Component/VMDLParticleEmitterComponent.h"

#include "Gameplay/Camera/Camera.h"
#include "Rendering/Core/RenderContext.h"
#include "Rendering/Effect/EffectManager.h"

Matrix CreateBillboardTransform(const Matrix& source, const Camera& camera)
{
	Vector3 scale;
	Quaternion rotation;
	Vector3 position;
	Matrix matrix = source;
	if (!matrix.Matrix::Decompose(scale, rotation, position)) return source;

	const Vector3 right = camera.GetRight() * scale.x;
	const Vector3 up = camera.GetUp() * scale.y;
	const Vector3 front = camera.GetFront() * scale.z;
	return Matrix(
		right.x, right.y, right.z, 0.0f,
		up.x, up.y, up.z, 0.0f,
		front.x, front.y, front.z, 0.0f,
		position.x, position.y, position.z, 1.0f);
}

VMDLParticleEmitterComponent::VMDLParticleEmitterComponent(Object* owner, VMDLModel* model,
	const VMDLModel::VmdlParticleEmitter& settings, bool initiallyEmitting)
	: Component(owner), model(model), settings(settings)
{
	SetSettings(settings);
	SetEmitting(initiallyEmitting);
}

VMDLParticleEmitterComponent::~VMDLParticleEmitterComponent()
{
	if (!effect) return;
	for (const Playback& playback : playbacks)
		effect->Stop(playback.handle);
	playbacks.clear();
}

void VMDLParticleEmitterComponent::SetSettings(const VMDLModel::VmdlParticleEmitter& value)
{
	const bool restart = emitting;
	Stop();
	settings = value;
	effect.reset();
	if (!settings.effekseerData.empty())
		effect = std::make_shared<Effect>(
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

void VMDLParticleEmitterComponent::Play()
{
	if (!effect || !effect->IsValid()) return;
	const Matrix transform = GetWorldTransform();
	const Effekseer::Handle handle = settings.modelDerived
		? effect->Play(Vector3::Zero)
		: EffectManager::Instance().PlayDetached(effect, transform);
	if (handle < 0) return;
	if (settings.modelDerived) effect->SetTransform(handle, transform);
	playbacks.push_back({handle, transform});
}

Matrix VMDLParticleEmitterComponent::GetWorldTransform() const
{
	Matrix transform = settings.transform.ToMatrix();
	if (!model || settings.nodeIndex < 0 ||
		settings.nodeIndex >= static_cast<int>(model->GetNodes().size()))
		return transform;
	return model->GetScaledAttachmentTransform(
		transform * model->GetNodes()[settings.nodeIndex].worldTransform);
}

void VMDLParticleEmitterComponent::Stop()
{
	if (!effect) return;
	for (const Playback& playback : playbacks)
		effect->StopRoot(playback.handle);
	playbacks.clear();
}

void VMDLParticleEmitterComponent::OnLateUpdate()
{
	auto manager = EffectManager::Instance().GetEffekseerManager();
	std::erase_if(playbacks, [&manager](const Playback& playback)
	{
		return !manager->Exists(playback.handle);
	});
	if (!settings.modelDerived || !effect) return;
	const Matrix transform = GetWorldTransform();
	for (const Playback& playback : playbacks)
		effect->SetTransform(playback.handle, transform);
}

void VMDLParticleEmitterComponent::RenderParticles(const RenderContext& rc)
{
	if (!settings.billboard || !effect || !rc.camera) return;
	const Matrix modelTransform = settings.modelDerived ? GetWorldTransform() : Matrix::Identity;
	for (const Playback& playback : playbacks)
	{
		const Matrix& transform = settings.modelDerived ? modelTransform : playback.initialTransform;
		effect->SetTransform(playback.handle, CreateBillboardTransform(transform, *rc.camera));
	}
}

void VMDLParticleEmitterComponent::OnDrawGUI()
{
	ImGui::Text("Effect: %s", settings.name.c_str());
	ImGui::Text("EFKPKG: %s", settings.effekseerFileName.c_str());
	bool active = emitting;
	if (ImGui::Checkbox((const char*)u8"再生中", &active)) SetEmitting(active);
	if (ImGui::Button((const char*)u8"再生")) Play();
}
