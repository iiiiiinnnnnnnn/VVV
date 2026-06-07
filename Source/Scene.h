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
#include "PostEffect.h"

class Scene
{
public:
	Scene(const std::string& name = "");

	virtual ~Scene() = default;

	virtual void Update();
	virtual void Render();

	Camera* GetCamera() { return &camera; }
	CameraController* GetNowCameraController() const;
	int GetNowCameraControllerIndex() const { return nowCameraControllerIndex; }
	const LightData& GetLightManager() const { return lightData; }
	const RenderSettings& GetRenderSettings() const { return renderSettings; }

private:
	void DrawGUI(RenderContext& rc);

protected:
	virtual void OnUpdate() {}
	virtual void OnDrawGUI() {}

	std::string										name;

	Camera											camera;
	std::vector<std::unique_ptr<CameraController>>	cameraControllers;
	int 											nowCameraControllerIndex = 0;
	PostEffect										postEffect;

	LightData										lightData;
	RenderSettings 									renderSettings;
	ShadowMapData									shadowMapData;
	IBLData											iblData;

	struct Actors
	{
		std::vector<std::shared_ptr<Actor>> data;
		void Register(std::shared_ptr<Actor> actor) { data.push_back(actor); }
		void Update()
		{
			for (auto& d : data)
			{
				d->Update();
			}
			// 削除フラグありのオブジェクトを削除
			data.erase(
				std::remove_if(data.begin(), data.end(),
				[](const std::shared_ptr<Actor>& a)
			{
				return a->IsPendingDestroy();
			}),
				data.end()
			);
		}
		void Render(const RenderContext& rc)
		{
			for (auto& d : data)
			{
				if (!d->IsPendingDestroy())
					d->Render(rc);
			}
		}
		void DrawGUI()
		{
			for (auto& d : data)
			{
				if (!d->IsPendingDestroy())
					d->DrawGUI();
			}
		}
	} actors;

	struct Widgets
	{
		std::vector<std::shared_ptr<Widget>> data;
		void Register(std::shared_ptr<Widget> widget) { data.push_back(widget); }
		void Update()
		{
			for (auto& d : data)
			{
				d->Update();
			}
			// 削除フラグありのオブジェクトを削除
			data.erase(
				std::remove_if(data.begin(), data.end(),
				[](const std::shared_ptr<Widget>& a)
			{
				return a->IsPendingDestroy();
			}),
				data.end()
			);
		}
		void Render(const RenderContext& rc)
		{
			for (auto& d : data)
			{
				if (!d->IsPendingDestroy())
					d->Render(rc);
			}
		}
		void DrawGUI()
		{
			for (auto& d : data)
			{
				if (!d->IsPendingDestroy())
					d->DrawGUI();
			}
		}
	} widgets;
};
