// Scene.h
#pragma once
#include <cstdio>
#include <memory>
#include <source_location>
#include <string_view>
#include <optional>
#include <string>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Rendering/Core/Graphics.h"
#include "Application/Input/Input.h"
#include "UI/Widget.h"
#include "Rendering/Core/RenderContext.h"
#include "Rendering/Shader/GamePostProcess.h"
#include "UI/WidgetManager.h"
#include "Gameplay/Stage/Stage01.h"

class Actor;
class Animator;
class VMDLModel;

class Scene
{
public:
	struct LogFormat
	{
		const char* text;
		std::source_location location;

		LogFormat(const char* value,
			std::source_location source = std::source_location::current())
			: text(value), location(source) {}
	};

	template<class... Args>
	static void Log(LogFormat format, Args... args)
	{
		const int length = std::snprintf(nullptr, 0, format.text, args...);
		if (length < 0) return;
		std::string message(static_cast<size_t>(length) + 1, '\0');
		std::snprintf(message.data(), message.size(), format.text, args...);
		message.resize(length);
		WriteLog(message, format.location);
	}

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
	static void WriteLog(std::string_view message, const std::source_location& location);

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

	// ゲーム内エディタ
	bool isCursorReleased = false;
	std::optional<MouseCursorMode> hudMouseCursorMode;
	bool showGameEditorGUI = true;
	float gameEditorLeftWidth = 600.0f;
	float gameEditorRightWidth = 680.0f;
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
