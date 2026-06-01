// Framework.cpp
#include "Framework.h"
#include "Graphics.h"
#include "ImGuiRenderer.h"
#include "Input.h"
#include "Time.h"

#include "PhysicsManager.h"
#include "TestPlayScene.h"

// 垂直同期間隔設定
static const int syncInterval = 1;

// コンストラクタ
Framework::Framework(HWND hWnd)
	: hWnd(hWnd)
{
	// シフトキーが押されている場合はコンソールを表示
#if _DEBUG
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
	{
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}
#endif

	// 入力初期化
	Input::Instance().Initialize(hWnd);

	// グラフィックス初期化
	Graphics::Instance().Initialize(hWnd);

	// IMGUI初期化
	ImGuiRenderer::Initialize(hWnd, Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());

	// 物理マネージャ初期化
	PhysicsManager::Instance().Initialize();

	// シーン初期化
	scene = std::make_unique<TestPlayScene>();
}

// デストラクタ
Framework::~Framework()
{
	// IMGUI終了化
	ImGuiRenderer::Finalize();

	// シーン終了化
	scene.reset();

	// 物理マネージャ終了化
	PhysicsManager::Instance().Finalize();
}

// 更新処理
void Framework::Update(float elapsedTime)
{
	// 時間更新処理
	Time::time += elapsedTime;
	Time::deltaTime = elapsedTime * Time::scale;
	Time::unscaledDeltaTime = elapsedTime;

	// 入力更新処理
	Input::Instance().Update();

	// IMGUIフレーム開始処理	
	ImGuiRenderer::NewFrame();

	// シーン更新処理
	scene->Update(elapsedTime);
}

// 描画処理
void Framework::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();

	// 画面クリア
	Graphics::Instance().Clear(0.5f, 0.5f, 0.5f, 1);

	// レンダーターゲット設定
	Graphics::Instance().SetRenderTargets();

	// シーン通常描画＆GUI描画処理
	// GUI描画もSceneに任せちゃうお(rc拾えるようにするため)
	scene->Render(elapsedTime);

	// IMGUI描画
	ImGuiRenderer::Render(dc);

	// 画面表示
	Graphics::Instance().Present(syncInterval);
}

// フレームレート計算
void Framework::CalculateFrameStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.
	static int frames = 0;
	static float time_tlapsed = 0.0f;

	frames++;

	// Compute averages over one second period.
	if ((timer.TimeStamp() - time_tlapsed) >= 1.0f)
	{
		float fps = static_cast<float>(frames); // fps = frameCnt / 1
		float mspf = 1000.0f / fps;
		std::ostringstream outs;
		outs.precision(6);
		outs << "FPS : " << fps << " / " << "Frame Time : " << mspf << " (ms)";
		SetWindowTextA(hWnd, outs.str().c_str());

		// Reset for next average.
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
