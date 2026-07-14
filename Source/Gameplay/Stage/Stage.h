// Stage.h

#pragma once

#include "Core/Object/Object.h"
#include "Core/Object/Transform.h"

#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Gameplay/Camera/CameraController.h"
#include "Rendering/Effect/ParticleSystem.h"

class Stage : public Object
{
public:
	Stage() = default;

	void Update() override
	{
		transform.Update();
		Object::Update();
		actorManager.Update();
		lightManager.Update();
	}

	void Render(const RenderContext& rc) override
	{
		Object::Render(rc);
		actorManager.Render(rc);
	}

	Transform* GetTransform() override { return &transform; }
	const Transform* GetTransform() const override { return &transform; }

	Camera* GetCamera()
	{
		return &camera;
	}

	CameraController* GetNowCameraController() const
	{
		if (cameraControllers.empty()) return nullptr;
		return cameraControllers[nowCameraControllerIndex].get();
	}
	std::vector<std::unique_ptr<CameraController>>& GetCameraControllers()
	{
		return cameraControllers;
	}

	int GetNowCameraControllerIndex() const
	{
		return nowCameraControllerIndex;
	}
	int& GetNowCameraControllerIndexRef() { return nowCameraControllerIndex; }

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
	Camera camera;
	std::vector<std::unique_ptr<CameraController>> cameraControllers;
	int nowCameraControllerIndex = 0;

	ActorManager actorManager;
	LightManager lightManager;
	std::unique_ptr<ParticleSystem> particleSystem;
};
