// Scene.h

#pragma once
#include <memory>
#include <vector>

#include "Common.h"
#include "Graphics.h"
#include "Input.h"
#include "Camera.h"
#include "Actor.h"
#include "Widget.h"
#include "CameraController.h"
#include "RenderContext.h"
#include "PhysicsManager.h"
#include "GamePostProcess.h"
#include "DynamicAnimationEditorWindow.h"
#include "ActorManager.h"
#include "WidgetManager.h"
#include "LightManager.h"

// Sceneへ渡す任意のデータ。
// 使用するScene側で必要な型へキャストして使う。
using SceneMessage = void*;

class Scene
{
public:
	Scene(SceneMessage message = nullptr);

	virtual ~Scene() = default;

	virtual void Update();
	virtual void Render();

	Camera* GetCamera()
	{
		return &camera;
	}

	CameraController* GetNowCameraController() const;

	int GetNowCameraControllerIndex() const
	{
		return nowCameraControllerIndex;
	}

	const LightManager& GetLightManager() const
	{
		return lightManager;
	}

	const RenderSettings& GetRenderSettings() const
	{
		return renderSettings;
	}

	SceneMessage GetSceneMessage() const
	{
		return message;
	}

private:
	void SwitchToDebugMode();
	void SwitchToPlayMode();
	void DrawGUI(RenderContext& rc);

protected:
	virtual void OnUpdate() {}
	virtual void OnDrawGUI() {}

	SceneMessage message = nullptr;

	bool showDynamicAnimationEditorWindow = false;
	DynamicAnimationEditorWindow dynamicAnimationEditorWindow;

	Camera camera;
	std::vector<std::unique_ptr<CameraController>> cameraControllers;
	int nowCameraControllerIndex = 0;
	bool isCursorReleased = false;

	Game::PostProcess postProcess;
	RenderSettings renderSettings;
	ShadowMapData shadowMapData;
	IBLData iblData;

	ActorManager actorManager;
	WidgetManager widgetManager;
	LightManager lightManager;
};
