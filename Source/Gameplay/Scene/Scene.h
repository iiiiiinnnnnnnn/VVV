#pragma once
#include <memory>
#include <optional>
#include <string>
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

class Scene
{
public:
	enum class MouseCursorMode
	{
		VisibleFree,
		HiddenFree,
		HiddenLocked,
	};

	Scene() = default;

	virtual ~Scene() = default;

	virtual void Update();
	virtual void Render();
	CameraController* GetActiveCameraController() const;
	Stage* GetCurrentStage() { return currentStage.get(); }
	void RegisterWidget(std::shared_ptr<Widget> widget)
	{
		widgetManager.Register(std::move(widget));
	}

	const RenderSettings& GetRenderSettings() const
	{
		return renderSettings;
	}

private:
	void ApplyMouseCursorMode();
	void SwitchToDebugMode();
	void SwitchToPlayMode();
	void SelectPausedActor();
	void DrawGUI(RenderContext& rc);

protected:
	virtual MouseCursorMode GetMouseCursorMode() const = 0;
	void SetHudMouseCursorMode(MouseCursorMode mode) { hudMouseCursorMode = mode; }
	void ClearHudMouseCursorMode() { hudMouseCursorMode.reset(); }
	virtual void OnUpdate() {}
	virtual void ToggleDebugDisplay();
	virtual void ConfigureRenderSettings(RenderSettings&) {}
	virtual void OnRender(RenderContext& rc) {}
	virtual void OnDrawGUI() {}
	virtual bool ShouldUpdateWorld() const { return true; }

	virtual bool UsesGameDebugGUI() const { return true; }
	virtual bool OnRequestExit() { return true; }

	std::unique_ptr<Stage> currentStage;

	bool showDynamicAnimationEditorWindow = false;
	bool showPhysicsLayerWindow = false;
	DynamicAnimationEditorWindow dynamicAnimationEditorWindow;

	// ゲーム内エディタ
	bool isCursorReleased = false;
	std::optional<MouseCursorMode> hudMouseCursorMode;
	bool showGameEditorGUI = true;
	float gameEditorLeftWidth = 600.0f;
	float gameEditorRightWidth = 680.0f;
	std::string pendingStagePath;

	Game::PostProcess postProcess;
	RenderSettings renderSettings;
	ShadowMapData shadowMapData;
	IBLData iblData;

	WidgetManager widgetManager;
};
