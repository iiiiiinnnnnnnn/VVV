#include <ImGuiToolKit/ImGuiToolKit.h>
#include "Core/Foundation/Logger.h"
#include "Application/Bootstrap/Framework.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/ImGuiRenderer.h"
#include "Resource/ResourceManager.h"
#include <InputKit/InputKit.h>
#include "Application/Time/GameTime.h"
#include "Physics/Core/PhysicsManager.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TitleScene.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Gameplay/Scene/VstgEditorScene.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Audio/SoundSystem.h"
#include "Rendering/Effect/EffectManager.h"
#include "Gameplay/AI/LuaManager.h"
#include <ProfileKit/ProfileKit.h>
#include "Core/Foundation/Common.h"

// 垂直同期間隔設定
static constexpr UINT PresentSyncInterval = 0;
static constexpr double TargetFrameSeconds = 1.0 / 60.0;

// コンストラクタ
Framework::Framework(HWND hWnd, const wchar_t* commandLine) : hWnd(hWnd)
{
	Logger::Initialize();
	Log("Game started");

	// ImGuiとTimelineProfilerを使う表示バックエンドを登録する。
	Profiler::SetBackend(CreateProfilerTimelineBackend());

	// InputSystemの初期化時に、Input内の全定義を読み込む。
	const auto inputDirectory = ResourceManager::ResolveSourcePath(
		ResourceManager::Instance().ResolvePath("Resources/Input"));
	try
	{
		InputKit::InputSystem::Instance().Initialize(hWnd, inputDirectory);
	}
	catch (const std::exception& error)
	{
		Log(error.what());
		ImGuiToolKit::Alert(error.what(), "Input file error");
	}

	// ImGui操作中の入力抑制
	InputKit::InputSystem::Instance().SetFocusFilter([](bool ignoreUI) {
			const auto& io = ImGui::GetIO();
			return ignoreUI || (!io.WantTextInput &&
				(InputKit::InputSystem::Instance().GetMouse().IsCursorLocked() ||
					(!io.WantCaptureMouse && !io.WantCaptureKeyboard)));
		});

	// グラフィックス初期化
	Game::Graphics::Instance().Initialize(hWnd);
	InputKit::InputSystem::Instance().SetViewportSize(
		static_cast<UINT>(Game::Graphics::ScreenWidth), static_cast<UINT>(Game::Graphics::ScreenHeight));

	// サウンドシステム初期化
	SoundSystem::Instance().Initialize();
	EffectManager::Instance().Initialize();

	// 一覧と先読み対象を読み込む
	ResourceManager::Instance().PrepareGameResources();

	// エディタ用の設定初期化
	PhysicsLayerManager::Instance().Initialize();

	// IMGUI初期化
	ImGuiRenderer::Initialize(hWnd, Game::Graphics::Instance().GetDevice(),
		Game::Graphics::Instance().GetDeviceContext());

	// 物理マネージャ初期化
	PhysicsManager::Instance().Initialize();

	// 起動引数の先頭で初期シーンを選ぶ。エディター起動ではタイトルを生成しない。
	std::wistringstream arguments(commandLine ? commandLine : L"");
	std::wstring startupMode;
	arguments >> std::quoted(startupMode);

	// もしコマンドで実行されたとき引数によって起動するシーンを切り替え(一応)
	std::unique_ptr<Scene> initialScene;
	auto lowerArg = ToLowerWString(startupMode);
	if (!_wcsicmp(lowerArg.c_str(), L"vmdl"))
		initialScene = std::make_unique<VmdlEditorScene>();
	else if (!_wcsicmp(lowerArg.c_str(), L"vstg"))
		initialScene = std::make_unique<VstgEditorScene>();
	else
		initialScene = std::make_unique<TitleScene>();

	// シーン起動
	SceneManager::Instance().Initialize(std::move(initialScene));
}

// デストラクタ
Framework::~Framework()
{
	// シーンマネージャー終了化
	SceneManager::Instance().Finalize();

	// 物理マネージャ終了化
	PhysicsManager::Instance().Finalize();

	// IMGUI終了化
	ImGuiRenderer::Finalize();

	// サウンドシステム終了化
	SoundSystem::Instance().Finalize();
	EffectManager::Instance().Finalize();
}

// 更新処理
void Framework::Update(float elapsedTime)
{
	PROFILE_FUNCTION();

	// 時間更新処理
	Game::Time::time += elapsedTime;

	// 一時停止してなかったら時間を進める
	Game::Time::deltaTime = Game::Time::paused ? 0.0f : elapsedTime * Game::Time::scale;

	// 一時停止してなかったら時間を進める(スケール無視)
	Game::Time::unscaledDeltaTime = elapsedTime;

	// 時間スケール更新処理
	TimeScaleController::Update();

	// 入力更新処理
	InputKit::InputSystem::Instance().Update();

	// ImGuiのフレーム開始前にシーン切り替えと旧シーンの破棄を終える
	SceneManager::Instance().ApplyPendingChanges();

	// IMGUIフレーム開始処理
	ImGuiRenderer::NewFrame();

	// 同じLuaファイルの更新確認は、個体ごとではなくManagerで共有する。
	if (!SceneManager::Instance().IsLoading())
		LuaManager::Instance().Update();

	// シーン更新処理
	SceneManager::Instance().Update();

	// ロード中は旧シーンの処理を動かさず、ロードスレッドによる生成と競合させない。
	if (!SceneManager::Instance().IsLoading())
	{
		PhysicsManager::Instance().GetSceneContext().Simulate();
		SoundSystem::Instance().Update();
		Scene* scene = SceneManager::Instance().GetCurrentScene();
		Stage* stage = scene ? scene->GetCurrentStage() : nullptr;
		EffectManager::Instance().Update(stage ? stage->GetActiveCamera() : nullptr);
	}
}

// 描画処理
void Framework::Render(float elapsedTime)
{
	PROFILE_FUNCTION();

	ID3D11DeviceContext* dc = Game::Graphics::Instance().GetDeviceContext();

	// 画面クリア＆レンダーターゲット設定
	RenderTarget* backBuffer =
		Game::Graphics::Instance().GetFrameBuffer(Game::FrameBufferId::Display);
	backBuffer->Clear(dc, 0.025f, 0.04f, 0.065f, 1.0f);
	RenderOutput::Bind(dc, backBuffer, nullptr, false);

	// シーン通常描画＆GUI描画処理
	// GUI描画もSceneに任せちゃうお(rc拾えるようにするため)
	SceneManager::Instance().Render();

	// IMGUI描画
	ImGuiRenderer::Render(dc);

	// 画面表示
	Game::Graphics::Instance().Present(PresentSyncInterval);
}

// アプリケーションループ
int Framework::Run()
{
	MSG msg = {};
	HANDLE frameTimer = CreateWaitableTimerExW(
		nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	if (!frameTimer) frameTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
	LARGE_INTEGER performanceFrequency;
	LARGE_INTEGER nextFrameTime;
	QueryPerformanceFrequency(&performanceFrequency);
	QueryPerformanceCounter(&nextFrameTime);
	const LONGLONG targetFrameTicks =
		static_cast<LONGLONG>(performanceFrequency.QuadPart * TargetFrameSeconds);

	float sampleSeconds = 0.0f;
	double updateSeconds = 0.0, renderSeconds = 0.0;
	int sampleFrames = 0;
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			timer.Tick();

			float elapsedTime = timer.TimeInterval();
			LARGE_INTEGER updateBegin, renderBegin, renderEnd;

			QueryPerformanceCounter(&updateBegin);

			Profiler::BeginFrame(); // BEGIN PROFILER
			Update(elapsedTime); // UPDATE SCENE

			QueryPerformanceCounter(&renderBegin);

			Render(elapsedTime); // RENDER SCENE
			Profiler::EndFrame(); // END PROFILER

			QueryPerformanceCounter(&renderEnd);

			sampleSeconds += elapsedTime;
			updateSeconds += static_cast<double>(renderBegin.QuadPart - updateBegin.QuadPart) / performanceFrequency.QuadPart;
			renderSeconds += static_cast<double>(renderEnd.QuadPart - renderBegin.QuadPart) / performanceFrequency.QuadPart;
			++sampleFrames;
			if (sampleSeconds >= 2.0f)
			{
				if (sampleSeconds / sampleFrames > 0.025f || Game::Time::scale < 0.99f)

					Log("Performance: %.1f FPS, update %.2f ms, render/present %.2f ms, scale %.3f, paused %d",
						sampleFrames / sampleSeconds, updateSeconds * 1000.0 / sampleFrames,
						renderSeconds * 1000.0 / sampleFrames, Game::Time::scale, Game::Time::paused ? 1 : 0);

				sampleSeconds = 0.0f;
				updateSeconds = renderSeconds = 0.0;
				sampleFrames = 0;
			}

			nextFrameTime.QuadPart += targetFrameTicks;
			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);
			if (nextFrameTime.QuadPart > currentTime.QuadPart && frameTimer)
			{
				LARGE_INTEGER dueTime;
				dueTime.QuadPart = -(nextFrameTime.QuadPart - currentTime.QuadPart) * 10000000LL /
								   performanceFrequency.QuadPart;
				if (dueTime.QuadPart == 0) dueTime.QuadPart = -1;
				SetWaitableTimer(frameTimer, &dueTime, 0, nullptr, nullptr, FALSE);
				WaitForSingleObject(frameTimer, INFINITE);
			}
			else if (nextFrameTime.QuadPart <= currentTime.QuadPart)
			{
				nextFrameTime = currentTime;
			}
		}
	}
	if (frameTimer) CloseHandle(frameTimer);
	return static_cast<int>(msg.wParam);
}

// メッセージハンドラ
LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_MOUSEWHEEL)
	{
		InputKit::InputSystem::Instance().GetMouse().SetWheel(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
	}

	if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam)) return true;

	switch (msg)
	{
	case WM_CLOSE:
	{
		// ×ボタンとAlt+F4でも、シーンの未保存変更を確認してから終了する。
		static bool confirmingExit = false;
		if (confirmingExit) return 0;
		confirmingExit = true;
		Scene* scene = SceneManager::Instance().GetCurrentScene();
		const bool canExit = !scene || scene->ConfirmExit();
		confirmingExit = false;
		if (canExit) PostQuitMessage(0);
		return 0;
	}
	case WM_NCHITTEST:
	{
		const LRESULT hit = DefWindowProc(hWnd, msg, wParam, lParam);
		if (Game::Graphics::Instance().IsWindowMovementLocked() && hit == HTCAPTION)
			return HTCLIENT;
		return hit;
	}
	case WM_SYSCOMMAND:
	{
		const WPARAM command = wParam & 0xFFF0;

		if (Game::Graphics::Instance().IsWindowMovementLocked())
		{
			if (command == SC_MOVE || command == SC_SIZE || command == SC_MAXIMIZE)
			{
				return 0;
			}

			// 最大化状態からの「元のサイズに戻す」は禁止するが、
			// 最小化状態からの復帰は許可する
			if (command == SC_RESTORE && !IsIconic(hWnd))
			{
				return 0;
			}
		}

		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	case WM_SIZE:
	{
		if (wParam != SIZE_MINIMIZED)
			Game::Graphics::Instance().Resize(LOWORD(lParam), HIWORD(lParam));
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc;
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CREATE:
		break;
	case WM_ENTERSIZEMOVE:
		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
		timer.Stop();
		break;
	case WM_EXITSIZEMOVE:
		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
		timer.Start();
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}
