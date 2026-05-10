// Scene.h

#pragma once

#include "Common.h"
#include "Graphics.h"
#include "Input.h"
#include "Camera.h"
#include "Light.h"
#include "Actor.h"
#include "Widget.h"
#include "CameraController.h"
#include "RenderContext.h"
#include "PhysicsManager.h"

class Scene
{
public:
	Scene();

	virtual ~Scene() = default;

	virtual void Update(float elapsedTime);
	virtual void Render(float elapsedTime);
	virtual void DrawGUI(float elapsedTime);

	Camera* GetCamera() { return &camera; }
	CameraController* GetNowCameraController() const;
	int GetNowCameraControllerIndex() const { return nowCameraControllerIndex; }
	const LightManager& GetLightManager() const { return lightManager; }
	const RenderSettings& GetRenderSettings() const { return renderSettings; }

protected:
	virtual void OnUpdate(float elapsedTime) {}
	virtual void OnRender(RenderContext& rc, float elapsedTime) {}
	virtual void OnDrawGUI(float elapsedTime) {}

	Camera											camera;
	std::vector<std::unique_ptr<CameraController>>	cameraControllers;
	int 											nowCameraControllerIndex = 0;
	LightManager									lightManager;
	RenderSettings 									renderSettings;

	struct Actors
	{
		std::vector<std::shared_ptr<Actor>> data;
		void push_back(std::shared_ptr<Actor> actor) { data.push_back(actor); }
		void Update(float elapsedTime) {
			for (auto& a : data) {
				a->Update(elapsedTime);
			}
		}
		void Render(const RenderContext& rc, float elapsedTime) {
			for (auto& a : data) {
				a->Render(rc, elapsedTime);
			}
		}
		void DrawGUI(float elapsedTime) {
			for (auto& a : data) {
				a->DrawGUI(elapsedTime);
			}
		}
	} actors;

	struct Widgets
	{
		std::vector<std::shared_ptr<Widget>> data;
		void push_back(std::shared_ptr<Widget> widget) { data.push_back(widget); }
		void Update(float elapsedTime) {
			for (auto& a : data) {
				a->Update(elapsedTime);
			}
		}
		void Render(const RenderContext& rc, float elapsedTime) {
			for (auto& a : data) {
				a->Render(elapsedTime);
			}
		}
		void DrawGUI(float elapsedTime) {
			for (auto& a : data) {
				a->DrawGUI(elapsedTime);
			}
		}
	} widgets;
};
