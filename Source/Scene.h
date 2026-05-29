// Scene.h

#pragma once

#include "Common.h"
#include "Graphics.h"
#include "Input.h"
#include "Camera.h"
#include "Actor.h"
#include "Widget.h"
#include "CameraController.h"
#include "RenderContext.h"
#include "PhysicsManager.h"

class Scene
{
public:
	Scene(const std::string& name = "");

	virtual ~Scene() = default;

	virtual void Update(float elapsedTime);
	virtual void Render(float elapsedTime);

	Camera* GetCamera() { return &camera; }
	CameraController* GetNowCameraController() const;
	int GetNowCameraControllerIndex() const { return nowCameraControllerIndex; }
	const LightData& GetLightManager() const { return lightData; }
	const RenderSettings& GetRenderSettings() const { return renderSettings; }

protected:
	virtual void OnUpdate(float elapsedTime) {}
	virtual void OnDrawGUI(float elapsedTime) {}

	std::string										name;

	Camera											camera;
	std::vector<std::unique_ptr<CameraController>>	cameraControllers;
	int 											nowCameraControllerIndex = 0;

	LightData										lightData;
	RenderSettings 									renderSettings;
	ShadowMapData									shadowMapData;
	IBLData											iblData;

	struct Actors
	{
		std::vector<std::shared_ptr<Actor>> data;
		void Register(std::shared_ptr<Actor> actor) { data.push_back(actor); }
		void Update(float elapsedTime) {
			for (auto& d : data) {
				d->Update(elapsedTime);
			}
		}
		void Render(const RenderContext& rc, float elapsedTime) {
			for (auto& d : data) {
				d->Render(rc, elapsedTime);
			}
		}
		void DrawGUI(float elapsedTime) {
			for (auto& d : data) {
				d->DrawGUI(elapsedTime);
			}
		}
	} actors;

	struct Widgets
	{
		std::vector<std::shared_ptr<Widget>> data;
		void Register(std::shared_ptr<Widget> widget) { data.push_back(widget); }
		void Update(float elapsedTime) {
			for (auto& d : data) {
				d->Update(elapsedTime);
			}
		}
		void Render(const RenderContext& rc, float elapsedTime) {
			for (auto& d : data) {
				d->Render(rc, elapsedTime);
			}
		}
		void DrawGUI(float elapsedTime) {
			for (auto& d : data) {
				d->DrawGUI(elapsedTime);
			}
		}
	} widgets;
};
