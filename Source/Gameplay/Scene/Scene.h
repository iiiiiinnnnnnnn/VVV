// Scene.h
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Rendering/Core/Graphics.h"
#include <InputKit/InputKit.h>
#include "UI/Widget.h"
#include "Rendering/Core/RenderContext.h"
#include "Rendering/Shader/GamePostProcess.h"
#include "UI/WidgetManager.h"
#include "Gameplay/Stage/Stage01.h"

class Actor;
class Light;
class Animator;
class VMDLModel;

class Scene
{
public:
	enum class MouseCursorMode
	{
		VisibleFree,
		HiddenFree,
		HiddenLocked,
	};

	Scene();
	// 初期化・終了処理は一度だけ実行する。再使用時は新しいインスタンスを生成する。
	void Init();
	void Uninit();

	virtual ~Scene();

	void OpenAnimatorEditor(const std::string& path = {}, std::shared_ptr<VMDLModel> model = {},
		std::weak_ptr<Actor> liveOwner = {});
	void DrawAnimatorEditor();

	bool ConfirmExit() { return OnRequestExit(); }
	void CapturePreviewEnvironment();
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

	bool initialized = false;
	bool uninitialized = false;
	std::unique_ptr<Animator> animatorEditor;
	std::weak_ptr<Actor> liveAnimatorOwner;
	std::weak_ptr<Actor> pendingLiveAnimatorOwner;
	std::string pendingAnimatorPath;
	std::shared_ptr<VMDLModel> pendingAnimatorModel;
	bool animatorOpenRequested = false;

	void ApplyMouseCursorMode();
	void SwitchToDebugMode();
	void SwitchToPlayMode();
	void SelectPausedActor();
	void DrawGUI(RenderContext& rc);

protected:
	void DrawWindowMenu();
	void DrawEditorText(const std::string& title = {}, bool alignRight = true);
	void DrawToolWindows();
	virtual MouseCursorMode GetMouseCursorMode() const = 0;
	void SetHudMouseCursorMode(MouseCursorMode mode) { hudMouseCursorMode = mode; }
	void ClearHudMouseCursorMode() { hudMouseCursorMode.reset(); }
	virtual void OnInit() {}
	virtual void OnUninit() {}
	virtual void OnUpdate() {}
	virtual void ToggleDebugDisplay();
	virtual void ConfigureRenderSettings(RenderSettings&) {}
	virtual void ConfigurePostProcess(Game::PostProcess&) {}
	virtual bool ShouldApplyGameplayPostEffects() const { return true; }
	virtual void OnRender(RenderContext& rc) {}
	virtual void OnDrawGUI() {}
	virtual void OnDrawOverlayGUI() {}
	virtual bool ShouldRenderWorldEffects() const { return true; }
	virtual bool ShouldUpdateWorld() const { return true; }

	virtual bool UsesGameDebugGUI() const { return true; }
	virtual bool OnRequestExit() { return true; }

	std::unique_ptr<Stage> currentStage;

	bool showPhysicsLayerWindow = false;
	bool showProfilerWindow = false;
	bool showInputSystemWindow = false;

	// ゲーム内エディタ
	bool isCursorReleased = false;
	std::optional<MouseCursorMode> hudMouseCursorMode;
	bool showGameEditorGUI = true;
	float gameEditorLeftWidth = 320.0f;
	float gameEditorRightWidth = 440.0f;
	float gameEditorPanelHeight = 0.0f;
	bool gameEditorLayoutLoaded = false;
	int hierarchyTab = 0;
	int inspectorTab = 0;
	int sceneSettingsPage = 0;
	int editorSelectionType = -1;
	bool requestInspectorTab = false;
	Light* selectedLight = nullptr;
	char hierarchySearch[128] = {};
	std::string pendingStagePath;

	static std::unique_ptr<LightManager> previewLighting;
	static std::unique_ptr<Game::PostProcess> previewPostProcess;
	static RenderSettings previewRenderSettings;
	Game::PostProcess postProcess;
	RenderSettings renderSettings;
	ShadowMapData shadowMapData;
	IBLData iblData;

	WidgetManager widgetManager;
};
