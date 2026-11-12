#pragma once
#include <memory>
#include <algorithm>
#include <cmath>
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/CameraMoveSpeedOverlay.h"
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
	static constexpr float CameraFovStep = 2.0f;
	static constexpr float CameraMinimumFov = 10.0f;
	static constexpr float CameraMaximumFov = 120.0f;
	static constexpr float CameraSpeedStep = 1.1f;
	static constexpr float CameraMinimumSpeed = 0.1f;
	static constexpr float CameraMaximumSpeed = 500.0f;

	void UpdateCamFov(Camera& camera, bool acceptsInput)
	{
		const auto& io = ImGui::GetIO();
		if (acceptsInput && io.KeyCtrl && io.MouseWheel != 0.0f && !io.WantTextInput &&
			InputKit::InputSystem::IsFocusedWindow(true))
		{
			camera.SetVerticalFovDegrees(std::clamp(camera.GetVerticalFovDegrees() -
				io.MouseWheel * CameraFovStep, CameraMinimumFov, CameraMaximumFov));
			cameraSpeedOverlayTimer.Reset(0.0f);
			CameraMoveSpeedOverlay::Show(cameraFovOverlayTimer);
		}
		CameraMoveSpeedOverlay::Draw(camera.GetVerticalFovDegrees(), cameraFovOverlayTimer, "FOV");
	}

	void UpdateCamSpeed(float& speed, bool acceptsInput)
	{
		const auto& io = ImGui::GetIO();
		if (acceptsInput && !io.KeyCtrl && io.MouseWheel != 0.0f && !io.WantTextInput &&
			ImGui::IsMouseDown(ImGuiMouseButton_Right) && InputKit::InputSystem::IsFocusedWindow(true))
		{
			speed = std::clamp(speed * std::pow(CameraSpeedStep, io.MouseWheel),
				CameraMinimumSpeed, CameraMaximumSpeed);
			cameraFovOverlayTimer.Reset(0.0f);
			CameraMoveSpeedOverlay::Show(cameraSpeedOverlayTimer);
		}
		CameraMoveSpeedOverlay::Draw(speed, cameraSpeedOverlayTimer, "Speed");
	}

	Game::Timer cameraFovOverlayTimer{0.0f};
	Game::Timer cameraSpeedOverlayTimer{0.0f};
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
	int gameEditorResizeEdge = 0;
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
