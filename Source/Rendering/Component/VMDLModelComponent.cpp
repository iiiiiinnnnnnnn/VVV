#include "Rendering/Component/VMDLModelComponent.h"
#include <cstring>
#include "Animation/Animator.h"
#include "Animation/SpringBone.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Actor/Actor.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "Rendering/Component/VMDLParticleEmitterComponent.h"
#include "Audio/SoundSystem.h"
#include "Audio/SoundTrackRegistry.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/Scene.h"
#include "Gameplay/Stage/Stage.h"
#include "Gameplay/Camera/Camera.h"
#include "Core/Foundation/Easing.h"
#include "Resource/MeshCache.h"
#include "Resource/ResourceManager.h"
#include "IconsFontAwesome5.h"

VMDLModelComponent::VMDLModelComponent(Object* owner, std::shared_ptr<VMDLModel> model,
	ModelShaderId shaderId, VMatRenderParams renderParams)
	: Component(owner), model(model), shaderId(shaderId), renderParams(std::move(renderParams))
{
	// エラー用
	dynamic_cast<Actor*>(owner);

	if (model)
	{
		const Matrix placementTransform =
			Matrix::CreateTranslation(model->GetVmdlExtensionData().rootOffset);
		model->UpdateTransform(placementTransform);
	}
}

void VMDLModelComponent::OnAwake()
{
	BuildAttachments();
}

void VMDLModelComponent::BuildAttachments()
{
	if (attachmentsBuilt || !model) return;
	attachmentsBuilt = true;

	const auto& data = model->GetVmdlExtensionData();
	attachmentColliders.assign(data.colliders.size(), nullptr);
	for (int colliderIndex = 0; colliderIndex < static_cast<int>(data.colliders.size());
		++colliderIndex)
	{
		const auto& value = data.colliders[colliderIndex];
		if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size()))
			continue;

		const Matrix offset = Matrix::CreateFromYawPitchRoll(RAD(value.rotation.y),
								  RAD(value.rotation.x), RAD(value.rotation.z)) *
							  Matrix::CreateTranslation(value.center);

		const LayerId colliderLayer = value.layer >= 0 && value.layer < EditableLayerCount
										  ? static_cast<LayerId>(value.layer)
										  : attachmentLayerId;
		auto* collider = owner->AddComponent<VMDLColliderComponent>(colliderLayer, model.get(),
			value.nodeIndex, value.shape, value.size, offset, nullptr, value.trigger);

		collider->SetName(value.name);
		collider->SetActive(model->GetColliderInitialActive(colliderIndex));
		attachmentColliders[colliderIndex] = collider;
	}

	std::vector<SpringBone::SpringCapsule> springColliders;
	for (const auto& value : data.springColliders)
	{
		SpringBone::SpringCapsule collider;
		collider.start = value.offsetPosition;
		collider.end = value.offsetPosition;
		collider.radius = value.radius;
		collider.nodeIndex = value.nodeIndex;
		springColliders.push_back(collider);
	}
	for (const auto& value : data.springs)
	{
		if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size()))
			continue;
		auto* spring = owner->AddComponent<SpringBone>(attachmentLayerId, model.get(),
			value.nodeIndex, springColliders, value.stiffness, value.drag);
		spring->SetName(value.name);
	}

	const auto& trailData = model->GetVmdlTrailData();
	if (buildEmbeddedTrails)
	{
		attachmentTrails.assign(trailData.trails.size(), nullptr);
		for (int trailIndex = 0; trailIndex < static_cast<int>(trailData.trails.size()); ++trailIndex)
		{
			const auto& value = trailData.trails[trailIndex];
			if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size()))
				continue;
			auto* trail = owner->AddComponent<TrailRenderComponent>(model.get(), value.nodeIndex,
				value.rootOffset, value.tipOffset, value.color, value.tipRatio,
				std::max(0.01f, value.lifeTime), std::max(2, value.maxPoints), value.offsetAngle);
			if (!model->GetTrailInitialActive(trailIndex)) trail->StopTrail();
			attachmentTrails[trailIndex] = trail;
		}
	}

	const auto& particleData = model->GetVmdlParticleData();
	attachmentParticleEmitters.assign(particleData.emitters.size(), nullptr);
	for (int i = 0; i < static_cast<int>(particleData.emitters.size()); ++i)
	{
		const auto& value = particleData.emitters[i];
		if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size()))
			continue;
		attachmentParticleEmitters[i] = owner->AddComponent<VMDLParticleEmitterComponent>(
			model.get(), value, model->GetParticleInitialActive(i));
	}
}

PhysicsComponent* VMDLModelComponent::GetAttachmentCollider(const std::string& name) const
{
	for (VMDLColliderComponent* collider : attachmentColliders)
	{
		if (collider && collider->CompareName(name)) return collider;
	}
	for (VMDLColliderComponent* collider : attachmentColliders)
	{
		if (collider && ::_stricmp(collider->GetName().c_str(), name.c_str()) == 0) return collider;
	}
	return nullptr;
}

float VMDLModelComponent::BurstParticleEmitter(const std::string& name)
{
	for (VMDLParticleEmitterComponent* emitter : attachmentParticleEmitters)
	{
		if (!emitter || ::_stricmp(emitter->GetEmitterName().c_str(), name.c_str()) != 0)
			continue;
		emitter->Burst();
		return std::max(0.0f, emitter->GetMaximumLifetime());
	}
	return 0.0f;
}

bool VMDLModelComponent::SetParticleEmitterSettings(
	const std::string& name, const VMDLModel::VmdlParticleEmitter& settings)
{
	for (VMDLParticleEmitterComponent* emitter : attachmentParticleEmitters)
	{
		if (!emitter || ::_stricmp(emitter->GetEmitterName().c_str(), name.c_str()) != 0)
			continue;
		emitter->SetSettings(settings);
		return true;
	}
	return false;
}

void VMDLModelComponent::LateUpdate()
{
	Actor* actor = dynamic_cast<Actor*>(owner);

	if (!model) return;
	UpdateAnimationControls();
	SyncExternalMeshCaches();

	if (autoUpdateTransform)
		UpdateModelTransform(actor->transform.matrix);

	for (VMDLColliderComponent* collider : attachmentColliders)
	{
		if (collider) collider->UpdateFromNode();
	}
	UpdateSoundEvents();
	UpdatePresentationEvents();
}

void VMDLModelComponent::UpdatePresentationEvents()
{
	if (!model) return;
	const auto& presentation = model->GetVmdlPresentationData();
	if (presentation.cameraShakeTracks.empty() && presentation.radialBlurTracks.empty()) return;
	if (!animator) animator = owner->GetComponent<Animator>();
	if (!animator || animator->IsDynamicMode()) return;

	int animationIndex = -1;
	float time = 0.0f;
	int nextAnimationIndex = -1;
	float nextTime = 0.0f;
	if (!animator->GetAnimationControlState(animationIndex, time, nextAnimationIndex, nextTime))
	{
		presentationAnimationIndex = -1;
		presentationAnimationTime = 0.0f;
		return;
	}
	if (nextAnimationIndex >= 0) { animationIndex = nextAnimationIndex; time = nextTime; }
	if (animationIndex < 0 || animationIndex >= static_cast<int>(model->GetAnimations().size())) return;

	Vector3 listenerPosition = dynamic_cast<Actor*>(owner)->transform.position;
	if (Scene* scene = SceneManager::Instance().GetCurrentScene())
		if (Stage* stage = scene->GetCurrentStage())
			if (Camera* camera = stage->GetActiveCamera()) listenerPosition = camera->GetEye();

	if (presentationAnimationIndex != animationIndex)
		PlayPresentationEvents(animationIndex, -0.0001f, time, listenerPosition);
	else if (time + 0.0001f >= presentationAnimationTime)
		PlayPresentationEvents(animationIndex, presentationAnimationTime, time, listenerPosition);
	else
	{
		PlayPresentationEvents(animationIndex, presentationAnimationTime,
			model->GetAnimations()[animationIndex].secondsLength, listenerPosition);
		PlayPresentationEvents(animationIndex, -0.0001f, time, listenerPosition);
	}
	presentationAnimationIndex = animationIndex;
	presentationAnimationTime = time;
}

void VMDLModelComponent::PlayPresentationEvents(
	int animationIndex, float beginTime, float endTime, const Vector3& listenerPosition)
{
	if (!model || endTime <= beginTime + 0.00001f || animationIndex < 0 ||
		animationIndex >= static_cast<int>(model->GetAnimations().size())) return;
	const auto& data = model->GetVmdlPresentationData();
	const std::string& animationName = model->GetAnimations()[animationIndex].name;
	const auto strengthAt = [this, &listenerPosition](int nodeIndex, float range, bool attenuate) {
		const Vector3 origin = nodeIndex >= 0 && nodeIndex < static_cast<int>(model->GetNodes().size())
			? model->GetNodes()[nodeIndex].worldTransform.Translation()
			: dynamic_cast<Actor*>(owner)->transform.position;
		const float distance = Vector3::Distance(origin, listenerPosition);
		if (distance > range) return 0.0f;
		if (!attenuate) return 1.0f;
		const float t = std::clamp(1.0f - distance / std::max(range, 0.01f), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	};
	for (const auto& track : data.cameraShakeTracks)
	{
		if (track.animationName != animationName) continue;
		for (const auto& key : track.keys)
		{
			if (key.seconds <= beginTime + 0.00001f || key.seconds > endTime + 0.00001f ||
				key.componentIndex < 0 || key.componentIndex >= static_cast<int>(data.cameraShakes.size())) continue;
			const auto& value = data.cameraShakes[key.componentIndex];
			const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
			if (strength > 0.0f) CameraEffectController::Request(value.duration, value.intensity * strength);
		}
	}
	for (const auto& track : data.radialBlurTracks)
	{
		if (track.animationName != animationName) continue;
		for (const auto& key : track.keys)
		{
			if (key.seconds <= beginTime + 0.00001f || key.seconds > endTime + 0.00001f ||
				key.componentIndex < 0 || key.componentIndex >= static_cast<int>(data.radialBlurs.size())) continue;
			const auto& value = data.radialBlurs[key.componentIndex];
			const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
			if (strength > 0.0f) PostProcessController::Instance().RequestThreaten(value.duration,
				value.power * strength, value.attackRate, Easing::Type::InSine, Easing::Type::OutCubic);
		}
	}
}

void VMDLModelComponent::UpdateSoundEvents()
{
	if (!model) return;
	const auto& soundData = model->GetVmdlSoundData();
	for (auto event = activeSoundEvents.begin(); event != activeSoundEvents.end();)
	{
		if (!SoundSystem::Instance().IsPlaying(event->voiceId) || event->sourceIndex < 0 ||
			event->sourceIndex >= static_cast<int>(soundData.sources.size()))
		{
			event = activeSoundEvents.erase(event);
			continue;
		}
		const auto& source = soundData.sources[event->sourceIndex];
		if (source.nodeIndex < 0 || source.nodeIndex >= static_cast<int>(model->GetNodes().size()) ||
			!SoundSystem::Instance().SetPosition(event->voiceId,
				model->GetNodes()[source.nodeIndex].worldTransform.Translation()))
		{
			event = activeSoundEvents.erase(event);
			continue;
		}
		++event;
	}
	if (soundData.tracks.empty()) return;
	if (!animator) animator = owner->GetComponent<Animator>();
	if (!animator || animator->IsDynamicMode()) return;

	int animationIndex = -1;
	float time = 0.0f;
	int nextAnimationIndex = -1;
	float nextTime = 0.0f;
	if (!animator->GetAnimationControlState(animationIndex, time, nextAnimationIndex, nextTime))
	{
		soundAnimationIndex = -1;
		soundAnimationTime = 0.0f;
		return;
	}
	if (nextAnimationIndex >= 0)
	{
		animationIndex = nextAnimationIndex;
		time = nextTime;
	}
	if (animationIndex < 0 || animationIndex >= static_cast<int>(model->GetAnimations().size())) return;

	if (soundAnimationIndex != animationIndex)
	{
		PlaySoundEvents(animationIndex, -0.0001f, time);
	}
	else if (time + 0.0001f >= soundAnimationTime)
	{
		PlaySoundEvents(animationIndex, soundAnimationTime, time);
	}
	else
	{
		PlaySoundEvents(animationIndex, soundAnimationTime,
			model->GetAnimations()[animationIndex].secondsLength);
		PlaySoundEvents(animationIndex, -0.0001f, time);
	}
	soundAnimationIndex = animationIndex;
	soundAnimationTime = time;
}

void VMDLModelComponent::PlaySoundEvents(int animationIndex, float beginTime, float endTime)
{
	if (endTime <= beginTime + 0.00001f) return;
	const auto& animations = model->GetAnimations();
	if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return;
	const auto& soundData = model->GetVmdlSoundData();
	for (const auto& track : soundData.tracks)
	{
		if (track.animationName != animations[animationIndex].name) continue;
		for (const auto& key : track.keys)
		{
			if (key.seconds <= beginTime + 0.00001f || key.seconds > endTime + 0.00001f ||
				key.sourceIndex < 0 || key.sourceIndex >= static_cast<int>(soundData.sources.size()))
				continue;
			const auto& source = soundData.sources[key.sourceIndex];
			if (source.track < 0 || source.track > SoundTrackRegistry::MaximumTrack) continue;
			const float pitchMin = std::min(source.pitchMin, source.pitchMax);
			const float pitchMax = std::max(source.pitchMin, source.pitchMax);
			const float pitch = pitchMax > pitchMin
				? Random::Range(pitchMin, pitchMax) : pitchMin;
			if (source.spatial)
			{
				if (source.nodeIndex < 0 || source.nodeIndex >= static_cast<int>(model->GetNodes().size()))
					continue;
				SoundSystem::SpatialOptions options;
				options.volume = std::max(0.0f, source.volume);
				options.pitch = pitch;
				options.minDistance = source.minDistance;
				options.maxDistance = source.maxDistance;
				options.lowPassHz = source.lowPassHz;
				options.farLowPassHz = source.farLowPassHz;
				options.reverbMix = source.reverbMix;
				const auto voiceId = SoundSystem::Instance().PlayTrack3DAt(source.track,
					model->GetNodes()[source.nodeIndex].worldTransform.Translation(), source.variant, options);
				if (voiceId != SoundSystem::InvalidVoiceId)
					activeSoundEvents.push_back({voiceId, key.sourceIndex});
			}
			else
			{
				SoundSystem::PlayOptions options;
				options.volume = std::max(0.0f, source.volume);
				options.pitch = pitch;
				SoundSystem::Instance().PlayTrack(source.track, source.variant, options);
			}
		}
	}
}

bool VMDLModelComponent::PlaySoundSource(
	const std::string& name, const Vector3* positionOverride)
{
	if (!model || name.empty()) return false;

	const auto& sources = model->GetVmdlSoundData().sources;
	const auto found = std::find_if(sources.begin(), sources.end(), [&name](const auto& source) {
		return source.name == name;
	});
	if (found == sources.end() || found->track < 0 ||
		found->track > SoundTrackRegistry::MaximumTrack)
	{
		return false;
	}

	const float pitchMin = std::min(found->pitchMin, found->pitchMax);
	const float pitchMax = std::max(found->pitchMin, found->pitchMax);
	const float pitch = pitchMax > pitchMin ? Random::Range(pitchMin, pitchMax) : pitchMin;
	if (!found->spatial)
	{
		SoundSystem::PlayOptions options;
		options.volume = std::max(0.0f, found->volume);
		options.pitch = pitch;
		return SoundSystem::Instance().PlayTrack(found->track, found->variant, options) !=
			SoundSystem::InvalidVoiceId;
	}

	Vector3 position;
	if (positionOverride)
	{
		position = *positionOverride;
	}
	else
	{
		if (found->nodeIndex < 0 || found->nodeIndex >= static_cast<int>(model->GetNodes().size()))
			return false;
		position = model->GetNodes()[found->nodeIndex].worldTransform.Translation();
	}

	SoundSystem::SpatialOptions options;
	options.volume = std::max(0.0f, found->volume);
	options.pitch = pitch;
	options.minDistance = found->minDistance;
	options.maxDistance = found->maxDistance;
	options.lowPassHz = found->lowPassHz;
	options.farLowPassHz = found->farLowPassHz;
	options.reverbMix = found->reverbMix;
	return SoundSystem::Instance().PlayTrack3DAt(
		found->track, position, found->variant, options) != SoundSystem::InvalidVoiceId;
}

bool VMDLModelComponent::PlayPresentation(
	const std::string& name, const Vector3& listenerPosition)
{
	if (!model) return false;
	const auto effectOrigin = [this](int nodeIndex) {
		if (nodeIndex >= 0 && nodeIndex < static_cast<int>(model->GetNodes().size()))
			return model->GetNodes()[nodeIndex].worldTransform.Translation();
		return dynamic_cast<Actor*>(owner)->transform.position;
	};
	const auto strengthAt = [&listenerPosition, &effectOrigin](
		int nodeIndex, float range, bool attenuate) {
		const float distance = Vector3::Distance(effectOrigin(nodeIndex), listenerPosition);
		if (distance > range) return 0.0f;
		if (!attenuate) return 1.0f;
		const float t = std::clamp(1.0f - distance / std::max(range, 0.01f), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	};

	bool played = false;
	const auto& data = model->GetVmdlPresentationData();
	for (const auto& value : data.cameraShakes)
	{
		if (!name.empty() && ::_stricmp(value.name.c_str(), name.c_str()) != 0) continue;
		const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
		if (strength <= 0.0f) continue;
		CameraEffectController::Request(value.duration, value.intensity * strength);
		played = true;
	}
	for (const auto& value : data.radialBlurs)
	{
		if (!name.empty() && ::_stricmp(value.name.c_str(), name.c_str()) != 0) continue;
		const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
		if (strength <= 0.0f) continue;
		PostProcessController::Instance().RequestThreaten(value.duration,
			value.power * strength, value.attackRate, Easing::Type::InSine, Easing::Type::OutCubic);
		played = true;
	}
	return played;
}

void VMDLModelComponent::UpdateModelTransform(const Matrix& actorTransform)
{
	if (!model) return;
	const Matrix placementTransform =
		Matrix::CreateRotationY(modelYawOffset) *
		Matrix::CreateTranslation(model->GetVmdlExtensionData().rootOffset);
	model->UpdateTransform(placementTransform * actorTransform);
}

void VMDLModelComponent::UpdateAnimationControls()
{
	if (!model) return;
	if (!animator) animator = owner->GetComponent<Animator>();
	if (!animator || animator->IsDynamicMode()) return;

	int animationIndex = -1;
	float time = 0.0f;
	int nextAnimationIndex = -1;
	float nextTime = 0.0f;
	if (!animator->GetAnimationControlState(
		animationIndex, time, nextAnimationIndex, nextTime))
	{
		if (animationControlsApplied) RestoreAnimationControls();
		return;
	}

	for (int i = 0; i < static_cast<int>(attachmentColliders.size()); ++i)
	{
		if (attachmentColliders[i])
		{
			const bool currentActive = model->EvaluateColliderActive(animationIndex, time, i);
			const bool nextActive = nextAnimationIndex >= 0 &&
				model->EvaluateColliderActive(nextAnimationIndex, nextTime, i);
			const auto layerGate =
				attachmentLayerEnabled.find(attachmentColliders[i]->GetLayerId());
			const bool layerEnabled =
				layerGate == attachmentLayerEnabled.end() || layerGate->second;
			attachmentColliders[i]->SetActive(layerEnabled && (currentActive || nextActive));
		}
	}
	for (int i = 0; i < static_cast<int>(attachmentTrails.size()); ++i)
	{
		if (!attachmentTrails[i]) continue;
		const bool currentActive = model->EvaluateTrailActive(animationIndex, time, i);
		const bool nextActive = nextAnimationIndex >= 0 &&
			model->EvaluateTrailActive(nextAnimationIndex, nextTime, i);
		if (currentActive || nextActive) attachmentTrails[i]->StartTrail();
		else attachmentTrails[i]->StopTrail();
	}
	for (int i = 0; i < static_cast<int>(attachmentParticleEmitters.size()); ++i)
	{
		if (!attachmentParticleEmitters[i]) continue;
		const bool currentActive = model->EvaluateParticleActive(animationIndex, time, i);
		const bool nextActive = nextAnimationIndex >= 0 &&
			model->EvaluateParticleActive(nextAnimationIndex, nextTime, i);
		attachmentParticleEmitters[i]->SetEmitting(currentActive || nextActive);
	}
	if (nextAnimationIndex >= 0) model->ApplyMorphAnimation(nextAnimationIndex, nextTime);
	else model->ApplyMorphAnimation(animationIndex, time);
	animationControlsApplied = true;
}

void VMDLModelComponent::RestoreAnimationControls()
{
	if (!model) return;
	for (int i = 0; i < static_cast<int>(attachmentColliders.size()); ++i)
	{
		if (attachmentColliders[i])
		{
			const auto layerGate =
				attachmentLayerEnabled.find(attachmentColliders[i]->GetLayerId());
			const bool layerEnabled =
				layerGate == attachmentLayerEnabled.end() || layerGate->second;
			attachmentColliders[i]->SetActive(
				layerEnabled && model->GetColliderInitialActive(i));
		}
	}
	for (int i = 0; i < static_cast<int>(attachmentTrails.size()); ++i)
	{
		if (!attachmentTrails[i]) continue;
		if (model->GetTrailInitialActive(i)) attachmentTrails[i]->StartTrail();
		else attachmentTrails[i]->StopTrail();
	}
	for (int i = 0; i < static_cast<int>(attachmentParticleEmitters.size()); ++i)
	{
		if (attachmentParticleEmitters[i])
			attachmentParticleEmitters[i]->SetEmitting(model->GetParticleInitialActive(i));
	}
	model->RestoreRuntimeMorphVisibility();
	animationControlsApplied = false;
}

void VMDLModelComponent::Render(const RenderContext& rc)
{
	if (model)
	{
		auto* modelRenderer = Game::Graphics::Instance().GetModelRenderer();
		modelRenderer->Draw(shaderId, model, &renderParams);
		for (const auto& [slot, meshCache] : meshCaches)
			modelRenderer->DrawMeshCache(shaderId, meshCache, model, &renderParams);
		for (const auto& [groupIndex, meshCache] : externalMeshCaches)
			modelRenderer->DrawMeshCache(shaderId, meshCache, model, &renderParams);
	}
}

void VMDLModelComponent::SyncExternalMeshCaches()
{
	if (!model) return;
	const auto& groups = model->GetExternalMeshGroups();
	const auto& modelMeshes = model->GetMeshes();
	for (int groupIndex = 0; groupIndex < static_cast<int>(groups.size()); ++groupIndex)
	{
		const auto& group = groups[groupIndex];
		bool active = false;
		for (int meshIndex : group.meshIndices)
			if (meshIndex >= 0 && meshIndex < static_cast<int>(modelMeshes.size()) &&
				modelMeshes[meshIndex].isDraw) { active = true; break; }

		auto loaded = externalMeshCaches.find(groupIndex);
		if (!active)
		{
			if (loaded != externalMeshCaches.end()) externalMeshCaches.erase(loaded);
			continue;
		}
		if (loaded == externalMeshCaches.end())
		{
			try
			{
				const std::filesystem::path resolved =
					ResourceManager::Instance().ResolvePath(group.path);
				auto cache = std::make_shared<MeshCache>(resolved, *model);
				loaded = externalMeshCaches.emplace(groupIndex, std::move(cache)).first;
			}
			catch (const std::exception& exception)
			{
				const std::string message = "Lazy VMSH load failed: " + group.path +
					" (" + exception.what() + ")\n";
				OutputDebugStringA(message.c_str());
				continue;
			}
		}

		auto& cacheMeshes = loaded->second->GetMeshes();
		for (auto& cacheMesh : cacheMeshes) cacheMesh.isDraw = false;
		for (size_t bindingSlot = 0; bindingSlot < group.meshIndices.size(); ++bindingSlot)
		{
			const int cacheSlot = bindingSlot < group.cacheMeshIndices.size()
				? group.cacheMeshIndices[bindingSlot]
				: static_cast<int>(bindingSlot);
			const int meshIndex = group.meshIndices[bindingSlot];
			if (cacheSlot < 0 || cacheSlot >= static_cast<int>(cacheMeshes.size())) continue;
			cacheMeshes[cacheSlot].isDraw = meshIndex >= 0 &&
				meshIndex < static_cast<int>(modelMeshes.size()) && modelMeshes[meshIndex].isDraw;
		}
	}
	for (auto it = externalMeshCaches.begin(); it != externalMeshCaches.end();)
	{
		if (it->first < 0 || it->first >= static_cast<int>(groups.size()))
			it = externalMeshCaches.erase(it);
		else ++it;
	}
}

bool VMDLModelComponent::EquipMeshCache(
	const std::string& slot, const std::string& path, const std::string& fallbackNodeName)
{
	if (!model || slot.empty() || path.empty()) return false;
	try
	{
		// 新装備の成功後に同じスロットを交換
		std::filesystem::path resolvedPath = ResourceManager::Instance().ResolvePath(path);
		if (!std::filesystem::exists(resolvedPath))
		{
			const std::filesystem::path dataPath = std::filesystem::path("Resources") / path;
			if (std::filesystem::exists(dataPath)) resolvedPath = dataPath;
		}
		auto loaded = std::make_shared<MeshCache>(resolvedPath, *model, fallbackNodeName);
		meshCaches[slot] = std::move(loaded);
		return true;
	}
	catch (const std::exception& exception)
	{
		const std::string message =
			"MeshCache equip failed: " + path + " (" + exception.what() + ")\n";
		OutputDebugStringA(message.c_str());
		return false;
	}
}

void VMDLModelComponent::UnequipMeshCache(const std::string& slot)
{
	// 参照を外してGPUメモリも解放
	meshCaches.erase(slot);
}

void VMDLModelComponent::SetMaterialParamsForAllMaterials(const VMatMaterialParams& params)
{
	if (!model) return;
	for (const VMDLModel::Material& material : model->GetMaterials())
		renderParams.materials[material.name] = params;
}

void VMDLModelComponent::DrawGUI()
{
	ImGui::Text("Mesh Caches: %zu", meshCaches.size());
	for (const auto& [slot, meshCache] : meshCaches)
		ImGui::BulletText("%s: %zu meshes", slot.c_str(), meshCache->GetMeshes().size());

	auto drawVector3 = [](const char* label, const Vector3& value) {
		ImGui::Text("%s: %.3f, %.3f, %.3f", label, value.x, value.y, value.z);
	};

	auto drawQuaternion = [](const char* label, const Quaternion& value) {
		ImGui::Text("%s: %.3f, %.3f, %.3f, %.3f", label, value.x, value.y, value.z, value.w);
	};

	auto drawMatrixTransform = [&](const char* label, const Matrix& matrix) {
		Vector3 scale;
		Vector3 position;
		Quaternion rotation;
		Matrix work = matrix;
		work.Decompose(scale, rotation, position);

		ImGui::SeparatorText(label);
		drawVector3("Position", position);
		drawQuaternion("Rotation", rotation);
		drawVector3("Scale", scale);
	};

	auto drawNodeTooltip = [&](VMDLModel::Node* node, int nodeIndex,
							   const std::string& meshIndices) {
		ImGui::BeginTooltip();

		ImGui::Text("Node[%d] %s", nodeIndex, node->name.c_str());
		ImGui::Text("Parent: %d", node->parentIndex);
		ImGui::Text("Children: %zu", node->children.size());
		ImGui::Text("Meshes: %s", meshIndices.empty() ? "None" : meshIndices.c_str());

		ImGui::SeparatorText("Local Node");
		drawVector3("Position", node->position);
		drawQuaternion("Rotation", node->rotation);
		drawVector3("Scale", node->scale);

		drawMatrixTransform("Local Matrix", node->localTransform);
		drawMatrixTransform("Global Matrix", node->globalTransform);
		drawMatrixTransform("World Matrix", node->worldTransform);

		ImGui::EndTooltip();
	};

	// ノードツリーを再帰的に描画する関数
	std::function<void(VMDLModel::Node*)> drawNodeTree = [&](VMDLModel::Node* node) {
		// 矢印をクリック、またはノードをダブルクリックで階層を開く
		ImGuiTreeNodeFlags nodeFlags =
			ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

		// 子がいない場合は矢印をつけない
		size_t childCount = node->children.size();
		if (childCount == 0)
		{
			nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		bool isAnyMeshHidden = false;
		std::string meshIndices = "";

		// このノードに関連するメッシュを探す
		for (int i = 0; i < model->GetMeshes().size(); i++)
		{
			const VMDLModel::Mesh& mesh = model->GetMeshes()[i];
			if (mesh.node == node)
			{
				if (!meshIndices.empty()) meshIndices += ",";
				meshIndices += std::to_string(i);

				if (!mesh.isDraw) isAnyMeshHidden = true;
			}
		}

		// ツリーノードを表示
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, isAnyMeshHidden ? 100 : 255));

		std::string meshStr = meshIndices.empty() ? "" : "{" + meshIndices + "}";
		int nodeIndex = static_cast<int>(node - model->GetNodes().data());

		// ノード名とインデックスに続けて、[x, y, z] 形式でポジションを表示
		bool opened = ImGui::TreeNodeEx(node, nodeFlags, "[%d]%s%s [%.2f, %.2f, %.2f]", nodeIndex,
			meshStr.c_str(), node->name.c_str(), node->position.x, node->position.y,
			node->position.z);

		ImGui::PopStyleColor();

		if (ImGui::IsItemHovered() || ImGui::IsItemFocused())
		{
			drawNodeTooltip(node, nodeIndex, meshIndices);
		}

		if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			for (VMDLModel::Mesh& mesh : model->GetMeshes())
			{
				if (mesh.node == node)
				{
					mesh.isDraw = !mesh.isDraw;
				}
			}
		}

		// 開かれている場合、子階層も同じ処理を行う
		if (opened && childCount > 0)
		{
			for (VMDLModel::Node* child : node->children)
			{
				drawNodeTree(child);
			}
			ImGui::TreePop();
		}
	};

	// すべてのルートノード（親を持たないノード）を起点に描画
	for (VMDLModel::Node& node : model->GetNodes())
	{
		if (node.parent == nullptr)
		{
			drawNodeTree(&node);
		}
	}

	if (ImGui::TreeNode(ICON_FA_PAINT_BRUSH " Materials"))
	{
		for (const VMDLModel::Material& material : model->GetMaterials())
		{
			if (ImGui::TreeNode(material.name.c_str()))
			{
				ImGui::Text("Base Color: %.3f, %.3f, %.3f, %.3f", material.baseColor.x,
					material.baseColor.y, material.baseColor.z, material.baseColor.w);
				ImGui::Text("Emissive Color: %.3f, %.3f, %.3f, %.3f", material.emissiveColor.x,
					material.emissiveColor.y, material.emissiveColor.z, material.emissiveColor.w);
				ImGui::Text("Metalness: %.3f", material.metalness);
				ImGui::Text("Roughness: %.3f", material.roughness);
				ImGui::Text("Occlusion: %.3f", material.occlusion);
				ImGui::Text("Occlusion Strength: %.3f", material.occlusionStrength);
				ImGui::Text("Shadow Strength: %.3f", material.shadowStrength);

				const auto it = renderParams.materials.find(material.name);
				if (it != renderParams.materials.end())
				{
					const VMatMaterialParams& params = it->second;
					ImGui::SeparatorText("Instance Overrides");
					if (params.baseColor)
						ImGui::Text("Base Color: %.3f, %.3f, %.3f, %.3f", params.baseColor->x,
							params.baseColor->y, params.baseColor->z, params.baseColor->w);
					if (params.emissionColor)
						ImGui::Text("Emission: %.3f, %.3f, %.3f, %.3f", params.emissionColor->x,
							params.emissionColor->y, params.emissionColor->z,
							params.emissionColor->w);
					if (params.metalness) ImGui::Text("Metalness: %.3f", *params.metalness);
					if (params.roughness) ImGui::Text("Roughness: %.3f", *params.roughness);
					if (params.occlusion) ImGui::Text("Occlusion: %.3f", *params.occlusion);
					if (params.occlusionStrength)
						ImGui::Text("Occlusion Strength: %.3f", *params.occlusionStrength);
					if (params.shadowStrength)
						ImGui::Text("Shadow Strength: %.3f", *params.shadowStrength);
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
}
