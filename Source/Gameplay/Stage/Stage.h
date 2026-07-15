// Stage.h

#pragma once

#include "Core/Object/Object.h"
#include "Core/Object/Transform.h"

#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/CameraController.h"
#include "Rendering/Effect/ParticleSystem.h"

#include <limits>
#include <algorithm>
#include <vector>

class Stage : public Object
{
public:
	Stage()
	{
		defaultCameraActor = std::make_unique<Actor>("Stage Camera");
		defaultCameraActor->AddComponent<Camera>();
	}

	void Update() override
	{
		transform.Update();
		Object::Update();
		defaultCameraActor->Update();
		actorManager.Update();
		NormalizeCameraPriorities();
		lightManager.Update();
	}

	void Render(const RenderContext& rc) override
	{
		Object::Render(rc);
		actorManager.Render(rc);
	}

	Transform* GetTransform() override { return &transform; }
	const Transform* GetTransform() const override { return &transform; }

	Camera* GetActiveCamera()
	{
		NormalizeCameraPriorities();

		Camera* result = defaultCameraActor->GetComponent<Camera>();
		int priority = result && result->IsActive()
			? result->GetPriority()
			: std::numeric_limits<int>::min();

		for (Actor* actor : actorManager.GetActors())
		{
			Camera* camera = actor->GetComponent<Camera>();
			if (!camera || !camera->IsActive()) continue;
			if (camera->GetPriority() <= priority) continue;

			result = camera;
			priority = camera->GetPriority();
		}

		return result;
	}

	const Camera* GetActiveCamera() const
	{
		return const_cast<Stage*>(this)->GetActiveCamera();
	}

	bool SetCameraPriority(Camera* camera, int priority)
	{
		if (!IsCameraRegistered(camera)) return false;

		camera->SetPriority(priority);
		NormalizeCameraPriorities();
		return true;
	}

	bool PromoteCamera(Camera* camera)
	{
		if (!IsCameraRegistered(camera)) return false;

		int highestPriority = -1;
		for (Camera* current : GetCameras())
			highestPriority = std::max(highestPriority, current->GetPriority());

		return SetCameraPriority(camera, highestPriority + 1);
	}

	void SetDebugCamera(Camera* camera)
	{
		debugCamera = camera;
	}

	Camera* GetDebugCamera() const
	{
		return debugCamera;
	}

	CameraController* GetActiveCameraController() const
	{
		Camera* camera = const_cast<Stage*>(this)->GetActiveCamera();
		return GetCameraController(camera);
	}

	CameraController* GetCameraController(Camera* camera) const
	{
		if (!camera) return nullptr;

		if (defaultCameraActor->GetComponent<Camera>() == camera)
			return defaultCameraActor->GetComponent<CameraController>();

		for (Actor* actor : actorManager.GetActors())
		{
			if (actor->GetComponent<Camera>() != camera) continue;
			return actor->GetComponent<CameraController>();
		}

		return nullptr;
	}

	Actor* GetDefaultCameraActor() { return defaultCameraActor.get(); }
	const Actor* GetDefaultCameraActor() const { return defaultCameraActor.get(); }

private:
	std::vector<Camera*> GetCameras() const
	{
		std::vector<Camera*> cameras;
		if (Camera* camera = defaultCameraActor->GetComponent<Camera>())
			cameras.push_back(camera);

		for (Actor* actor : actorManager.GetActors())
		{
			if (Camera* camera = actor->GetComponent<Camera>())
				cameras.push_back(camera);
		}

		return cameras;
	}

	void NormalizeCameraPriorities()
	{
		std::vector<Camera*> cameras = GetCameras();
		std::stable_sort(cameras.begin(), cameras.end(), [](Camera* left, Camera* right)
		{
			return left->GetNormalizedPriority() < right->GetNormalizedPriority();
		});

		for (Camera* camera : GetCameras())
		{
			if (!camera->HasPriorityChange()) continue;

			auto position = std::find(cameras.begin(), cameras.end(), camera);
			cameras.erase(position);

			int insertIndex = camera->GetPriority();
			if (insertIndex < 0) insertIndex = 0;
			if (insertIndex > static_cast<int>(cameras.size()))
				insertIndex = static_cast<int>(cameras.size());

			cameras.insert(cameras.begin() + insertIndex, camera);
		}

		for (int i = 0; i < static_cast<int>(cameras.size()); ++i)
			cameras[i]->SetNormalizedPriority(i);
	}

	bool IsCameraRegistered(Camera* camera) const
	{
		if (defaultCameraActor->GetComponent<Camera>() == camera)
			return true;

		for (Actor* actor : actorManager.GetActors())
			if (actor->GetComponent<Camera>() == camera) return true;

		return false;
	}

public:

	LightManager& GetLightManager()
	{
		return lightManager;
	}
	const LightManager& GetLightManager() const { return lightManager; }
	ActorManager& GetActorManager() { return actorManager; }
	const ActorManager& GetActorManager() const { return actorManager; }

	ParticleSystem* GetParticleSystem() { return particleSystem.get(); }

protected:
	Transform transform;
	std::unique_ptr<Actor> defaultCameraActor;
	Camera* debugCamera = nullptr;

	ActorManager actorManager;
	LightManager lightManager;
	std::unique_ptr<ParticleSystem> particleSystem;
};
