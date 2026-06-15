// Framework.cpp
#include "Framework.h"
#include "Graphics.h"
#include "ImGuiRenderer.h"
#include "GameInput.h"
#include "GameTime.h"
#include "PhysicsManager.h"
#include "SceneManager.h"
#include "TestPlayScene.h"

#if _DEBUG
#define SHOW_CONSOLE() if(!showedConsole){ AllocConsole(); freopen("CONOUT$", "w", stdout); freopen("CONOUT$", "w", stderr); showedConsole = true; }
#else
#define SHOW_CONSOLE()
#endif

// 垂直同期間隔設定
static const int syncInterval = 0;

// コンストラクタ
Framework::Framework(HWND hWnd)
	: hWnd(hWnd)
{
	if (GetAsyncKeyState(VK_F2) & 0x8000)
	{
		SHOW_CONSOLE();
	}

	// 入力初期化
	Game::Input::Instance().Initialize(hWnd);

	// グラフィックス初期化
	Game::Graphics::Instance().Initialize(hWnd);

	// IMGUI初期化
	ImGuiRenderer::Initialize(hWnd, Game::Graphics::Instance().GetDevice(), Game::Graphics::Instance().GetDeviceContext());

	// 物理マネージャ初期化
	PhysicsManager::Instance().Initialize();

	// シーンマネージャー初期化
	SceneManager::Instance().Initialize();

	// 最初のシーン初期化
	SceneManager::Instance().LoadScene<TestPlayScene>();
}

// デストラクタ
Framework::~Framework()
{
	// IMGUI終了化
	ImGuiRenderer::Finalize();

	// シーンマネージャー終了化
	SceneManager::Instance().Finalize();

	// 物理マネージャ終了化
	PhysicsManager::Instance().Finalize();

	// コンソール解放
	FreeConsole();
}

// 更新処理
void Framework::Update(float elapsedTime)
{
	if (GetAsyncKeyState(VK_F2) & 0x8000)
	{
		SHOW_CONSOLE();
	}

	elapsedTime = min(elapsedTime, 1.0f / 60.0f);

	// 時間更新処理
	Game::Time::time += elapsedTime;
	Game::Time::deltaTime = elapsedTime * Game::Time::scale;
	Game::Time::unscaledDeltaTime = elapsedTime;

	// 入力更新処理
	Game::Input::Instance().Update();

	// IMGUIフレーム開始処理
	ImGuiRenderer::NewFrame();

	// シーン更新処理
	SceneManager::Instance().Update();

	// 物理シミュレーション
	PhysicsManager::Instance().GetSceneContext().Simulate();
}

// 描画処理
void Framework::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Game::Graphics::Instance().GetDeviceContext();

	// 画面クリア＆レンダーターゲット設定
	RenderTarget* backBuffer = Game::Graphics::Instance().
		GetFrameBuffer(Game::FrameBufferId::Display);
	backBuffer->Clear(dc, 0.5f, 0.5f, 0.5f, 1);
	backBuffer->Activate(dc);

	// シーン通常描画＆GUI描画処理
	// GUI描画もSceneに任せちゃうお(rc拾えるようにするため)
	SceneManager::Instance().Render();

	// IMGUI描画
	ImGuiRenderer::Render(dc);

	// 画面表示
	Game::Graphics::Instance().Present(syncInterval);
}

// フレームレート計算
void Framework::CalculateFrameStats()
{
	// 1秒ごとにFPSを計算してウィンドウタイトルに表示
	static int frames = 0;
	static float time_tlapsed = 0.0f;
	frames++;

	if ((timer.TimeStamp() - time_tlapsed) >= 1.0f)
	{
		float fps = static_cast<float>(frames);
		float mspf = 1000.0f / fps;
		std::ostringstream outs;
		outs.precision(6);
		outs << "FPS : " << fps << " / " << "Frame Time : " << mspf << " (ms)";
		SetWindowTextA(hWnd, outs.str().c_str());

		// リセット
		frames = 0;
		time_tlapsed += 1.0f;
	}
}

// アプリケーションループ
int Framework::Run()
{
	MSG msg = {};

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
			CalculateFrameStats();

			float elapsedTime = timer.TimeInterval();
			Update(elapsedTime);
			Render(elapsedTime);
		}
	}
	return static_cast<int>(msg.wParam);
}

// メッセージハンドラ
LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
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
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) PostMessage(hWnd, WM_CLOSE, 0, 0);
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
