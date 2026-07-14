// Scene.h

#pragma once
#include <memory>
#include <vector>

#include "Common.h"
#include "Graphics.h"
#include "Input.h"
#include "Widget.h"
#include "RenderContext.h"
#include "GamePostProcess.h"
#include "DynamicAnimationEditorWindow.h"
#include "WidgetManager.h"
#include "Stage01.h"

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
	CameraController* GetNowCameraController() const;

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
	virtual void OnRender(RenderContext& rc) {}
	virtual void OnDrawGUI() {}

	std::unique_ptr<Stage> currentStage_;

	SceneMessage message = nullptr;

	bool showDynamicAnimationEditorWindow = false;
	DynamicAnimationEditorWindow dynamicAnimationEditorWindow;

	bool isCursorReleased = false;

	Game::PostProcess postProcess;
	RenderSettings renderSettings;
	ShadowMapData shadowMapData;
	IBLData iblData;

	WidgetManager widgetManager;
};
