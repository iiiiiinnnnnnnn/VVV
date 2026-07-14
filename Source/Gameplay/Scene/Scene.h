// Scene.h

#pragma once
#include <memory>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Rendering/Core/Graphics.h"
#include "Application/Input/Input.h"
#include "UI/Widget.h"
#include "Rendering/Core/RenderContext.h"
#include "Rendering/Shader/GamePostProcess.h"
#include "Application/Tools/DynamicAnimationEditorWindow.h"
#include "UI/WidgetManager.h"
#include "Gameplay/Stage/Stage01.h"

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
