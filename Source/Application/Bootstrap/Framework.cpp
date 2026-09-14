// Framework.cpp
#include "Application/Bootstrap/Framework.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/ImGuiRenderer.h"
#include "Resource/ResourceManager.h"
#include "GameInput.h"
#include "Application/Time/GameTime.h"
#include "Physics/Core/PhysicsManager.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Audio/SoundSystem.h"

// 垂直同期間隔設定
static constexpr UINT PresentSyncInterval = 0;
static constexpr double TargetFrameSeconds = 1.0 / 60.0;
static constexpr float DefaultClearRed = 0.025f;
static constexpr float DefaultClearGreen = 0.04f;
static constexpr float DefaultClearBlue = 0.065f;

// コンストラクタ
Framework::Framework(HWND hWnd) : hWnd(hWnd)
{
	// 入力初期化
	Game::Input::Instance().Initialize(hWnd);

	// グラフィックス初期化
	Game::Graphics::Instance().Initialize(hWnd);

	// サウンドシステム初期化
	SoundSystem::Instance().Initialize();

	// 一覧と先読み対象を読み込む
	ResourceManager::Instance().PrepareGameResources();

	// エディタ用の設定初期化
	PhysicsLayerManager::Instance().Initialize();

	// IMGUI初期化
	ImGuiRenderer::Initialize(hWnd, Game::Graphics::Instance().GetDevice(),
		Game::Graphics::Instance().GetDeviceContext());

	// 物理マネージャ初期化
	PhysicsManager::Instance().Initialize();

	// シーンマネージャー初期化
	SceneManager::Instance().Initialize();
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
}

// 更新処理
void Framework::Update(float elapsedTime)
{
	// 時間更新処理
	Game::Time::time += elapsedTime;

	// 一時停止してなかったら時間を進める
	Game::Time::deltaTime = Game::Time::paused ? 0.0f : elapsedTime * Game::Time::scale;
	
	Game::Time::unscaledDeltaTime = elapsedTime;
	TimeScaleController::Update();

	// 入力更新処理
	Game::Input::Instance().Update();

	// ImGuiのフレーム開始前にシーン切り替えと旧シーンの破棄を終える
	SceneManager::Instance().ApplyPendingChanges();

	// IMGUIフレーム開始処理
	ImGuiRenderer::NewFrame();

	// シーン更新処理
	SceneManager::Instance().Update();

	// 物理シミュレーション
	PhysicsManager::Instance().GetSceneContext().Simulate();

	// 最新の位置で3D音声を更新
	SoundSystem::Instance().Update();
}

// 描画処理
void Framework::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Game::Graphics::Instance().GetDeviceContext();

	// 画面クリア＆レンダーターゲット設定
	RenderTarget* backBuffer =
		Game::Graphics::Instance().GetFrameBuffer(Game::FrameBufferId::Display);
	backBuffer->Clear(
		dc, DefaultClearRed, DefaultClearGreen, DefaultClearBlue, 1.0f);
	backBuffer->Activate(dc);

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
			Update(elapsedTime);
			Render(elapsedTime);

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
		Game::Input::Instance().GetMouse().SetWheel(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
	}

	if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam)) return true;

	switch (msg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
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
