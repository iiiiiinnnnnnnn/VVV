// SceneManager.cpp
#include "Gameplay/Scene/SceneManager.h"

#include "Application/Time/GameTime.h"
#include "Gameplay/Scene/HitStop.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Audio/SoundSystem.h"
#include "Gameplay/Scene/LoadingScene.h"
#include "Gameplay/Scene/GameStartScene.h"
#include "Gameplay/Scene/TitleScene.h"
#include "Physics/Core/PhysicsManager.h"
#include "Rendering/Core/Graphics.h"
#include "imgui.h"
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/ResourceManager.h"
#endif

#include <stdexcept>
#include <fstream>
#include <chrono>

static void LogSceneLoadTime(const char* phase, std::chrono::steady_clock::time_point started)
{
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - started).count();
	const std::string message = std::string("[SceneLoad] ") + phase + ": " +
		std::to_string(milliseconds) + " ms\n";
	OutputDebugStringA(message.c_str());
	std::ofstream("SceneLoad.log", std::ios::app) << message;
}

namespace
{
	class ThreadSceneContextScope
	{
	public:
		ThreadSceneContextScope(
			PhysicsSceneContext* context)
		{
			PhysicsManager::Instance().
				SetThreadSceneContext(context);
		}

		~ThreadSceneContextScope()
		{
			PhysicsManager::Instance().
				SetThreadSceneContext(nullptr);
		}

		ThreadSceneContextScope(
			const ThreadSceneContextScope&) = delete;

		ThreadSceneContextScope& operator=(
			const ThreadSceneContextScope&) = delete;
	};

	std::string GetExceptionMessage(
		const std::exception_ptr& exception)
	{
		if (!exception)
		{
			return {};
		}

		try
		{
			std::rethrow_exception(exception);
		}
		catch (const std::exception& e)
		{
			return e.what();
		}
		catch (...)
		{
			return
				"Unknown exception occurred while loading scene.";
		}
	}
}

SceneManager::~SceneManager()
{
	Finalize();
}

void SceneManager::Initialize()
{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	currentScene = std::make_unique<GameStartScene>();
#else
	currentScene = std::make_unique<TitleScene>();
#endif
	loadProgress = 0.0f;
}

void SceneManager::Finalize()
{
	loadRequested.store(
		false,
		std::memory_order_release);

	{
		std::lock_guard<std::mutex> lock(loadMutex);
		pendingSceneFactory = {};
	}

	JoinLoadThread();

	std::unique_ptr<LoadedScene> sceneWaitingForDestruction;

	{
		std::lock_guard<std::mutex> lock(loadMutex);

		sceneWaitingForDestruction =
			std::move(loadedScene);

		loadException = nullptr;
		lastLoadError.clear();
	}

	loadFinished.store(
		false,
		std::memory_order_release);

	loading.store(
		false,
		std::memory_order_release);

	loadProgress = 0.0f;

	currentScene.reset();
	sceneWaitingForDestruction.reset();
}

void SceneManager::ApplyPendingChanges()
{
	ApplyLoadedScene();

	BeginPendingLoad();

	UpdateLoadProgress();
}

void SceneManager::Update()
{
	if (currentScene)
	{
		currentScene->Update();
	}
}

void SceneManager::Render()
{
	if (currentScene)
	{
		currentScene->Render();
	}
}

bool SceneManager::RequestLoadScene(
	SceneFactory sceneFactory)
{
	if (!sceneFactory)
	{
		return false;
	}

	if (loading.load(std::memory_order_acquire) ||
		loadRequested.load(std::memory_order_acquire))
	{
		return false;
	}

#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	// 次のシーンを作る前に変更分だけ更新
	auto& resources = ResourceManager::Instance();
	const auto refreshStarted = std::chrono::steady_clock::now();
	const bool refreshed = resources.RefreshResources();
	LogSceneLoadTime("Resource refresh", refreshStarted);
	if (!refreshed)
	{
		std::string error = "Resource cache refresh failed";
		if (!resources.GetErrors().empty()) error += ": " + resources.GetErrors().back();
		{
			std::lock_guard<std::mutex> lock(loadMutex);
			lastLoadError = error;
		}
		OutputDebugStringA(("[SceneManager] " + error + "\n").c_str());
		return false;
	}
#endif
	{
		std::lock_guard<std::mutex> lock(loadMutex);

		pendingSceneFactory = std::move(sceneFactory);
		lastLoadError.clear();
	}

	loadRequested.store(
		true,
		std::memory_order_release);

	return true;
}

void SceneManager::BeginPendingLoad()
{
	if (!loadRequested.exchange(
		false,
		std::memory_order_acq_rel))
	{
		return;
	}

	SceneFactory sceneFactory;

	{
		std::lock_guard<std::mutex> lock(loadMutex);
		sceneFactory = std::move(pendingSceneFactory);
		pendingSceneFactory = {};
	}

	if (!sceneFactory)
	{
		return;
	}

	JoinLoadThread();

	{
		std::lock_guard<std::mutex> lock(loadMutex);

		loadedScene.reset();
		loadException = nullptr;
		lastLoadError.clear();
	}

	loadFinished.store(
		false,
		std::memory_order_release);

	loading.store(
		true,
		std::memory_order_release);

	loadProgress = 0.0f;

	if (currentScene) currentScene->CapturePreviewEnvironment();
	const auto destructionStarted = std::chrono::steady_clock::now();
	currentScene.reset();
	LogSceneLoadTime("Previous scene destruction", destructionStarted);
	TimeScaleController::CancelAll();
	HitStop::CancelAll();
	Game::Time::scale = 1.0f;
	Game::Time::paused = false;
	Game::Time::deltaTime = Game::Time::unscaledDeltaTime;
	// 破棄されたシーンのBGM・SEをローディング画面や次シーンへ持ち越さない。
	SoundSystem::Instance().SetPaused(false);
	SoundSystem::Instance().StopAll();
	const auto loadingSceneStarted = std::chrono::steady_clock::now();
	currentScene = std::make_unique<LoadingScene>();
	LogSceneLoadTime("Loading screen creation", loadingSceneStarted);

	if (!StartLoadThread(std::move(sceneFactory)))
	{
		loading.store(
			false,
			std::memory_order_release);
	}
}

bool SceneManager::StartLoadThread(
	SceneFactory sceneFactory)
{
	try
	{
		loadThread = std::thread(
			[this, sceneFactory = std::move(sceneFactory)]() mutable
		{
			std::unique_ptr<LoadedScene> result;
			std::exception_ptr exception;
			const auto loadStarted = std::chrono::steady_clock::now();

			try
			{
				Game::Graphics& graphics = Game::Graphics::Instance();
				const auto skyStarted = std::chrono::steady_clock::now();
				const std::string skyMapName = graphics.GetSkyMapName();
				graphics.RefreshSkyMapList();
				if (!graphics.LoadSkyMap(skyMapName)) graphics.LoadSkyMap("Default");
				LogSceneLoadTime("Sky textures", skyStarted);

				result =
					std::make_unique<LoadedScene>();

				const auto physicsStarted = std::chrono::steady_clock::now();
				result->physicsContext =
					PhysicsManager::Instance().
					CreateSceneContext();
				LogSceneLoadTime("Physics scene creation", physicsStarted);

				{
					ThreadSceneContextScope contextScope(
						result->physicsContext.get());

					const auto sceneStarted = std::chrono::steady_clock::now();
					result->scene = sceneFactory();
					LogSceneLoadTime("Scene construction", sceneStarted);
				}

				if (!result->scene)
				{
					throw std::runtime_error(
						"Failed to create scene.");
				}
			}
			catch (...)
			{
				exception = std::make_exception_ptr(std::runtime_error(
					GetExceptionMessage(std::current_exception())));

				result.reset();
			}

			{
				std::lock_guard<std::mutex> lock(
					loadMutex);

				loadedScene = std::move(result);
				loadException = exception;
			}

			LogSceneLoadTime("Background load total", loadStarted);
			loadFinished.store(
				true,
				std::memory_order_release);
		});
	}
	catch (...)
	{
		const std::exception_ptr exception =
			std::current_exception();

		{
			std::lock_guard<std::mutex> lock(loadMutex);

			lastLoadError =
				GetExceptionMessage(exception);
		}

		loadFinished.store(
			false,
			std::memory_order_release);

		return false;
	}

	return true;
}

void SceneManager::UpdateLoadProgress()
{
	if (!loading.load(std::memory_order_acquire))
	{
		return;
	}

	const float target = 0.9f;
	const float interpolation = std::min(
		Game::Time::unscaledDeltaTime * 1.5f,
		1.0f);

	loadProgress +=
		(target - loadProgress) * interpolation;

	loadProgress = std::min(loadProgress, target);
}

std::string SceneManager::GetLastLoadError() const
{
	std::lock_guard<std::mutex> lock(loadMutex);
	return lastLoadError;
}

void SceneManager::ApplyLoadedScene()
{
	if (!loadFinished.load(std::memory_order_acquire))
	{
		return;
	}

	JoinLoadThread();

	std::unique_ptr<LoadedScene> result;
	std::exception_ptr exception;

	{
		std::lock_guard<std::mutex> lock(loadMutex);

		result = std::move(loadedScene);
		exception = loadException;
		loadException = nullptr;
	}

	loadFinished.store(
		false,
		std::memory_order_release);

	loading.store(
		false,
		std::memory_order_release);

	if (exception)
	{
		const std::string errorMessage =
			GetExceptionMessage(exception);

		{
			std::lock_guard<std::mutex> lock(loadMutex);
			lastLoadError = errorMessage;
		}

		const std::string output =
			"[SceneManager] LoadScene failed: " +
			errorMessage +
			"\n";

		OutputDebugStringA(output.c_str());
		return;
	}

	if (!result ||
		!result->scene ||
		!result->physicsContext)
	{
		std::lock_guard<std::mutex> lock(loadMutex);

		lastLoadError =
			"Loaded scene data is incomplete.";

		return;
	}

	loadProgress = 1.0f;

	currentScene.reset();
	TimeScaleController::CancelAll();
	HitStop::CancelAll();
	Game::Time::scale = 1.0f;
	Game::Time::paused = false;
	Game::Time::deltaTime = Game::Time::unscaledDeltaTime;
	SoundSystem::Instance().SetPaused(false);

	PhysicsManager::Instance().
		SetCurrentSceneContext(
			std::move(result->physicsContext));

	currentScene =
		std::move(result->scene);

	// 読み込み画面や起動メニューに残ったフォーカスを、新しいゲーム画面へ戻す
	if (ImGui::GetCurrentContext()) ImGui::SetWindowFocus(nullptr);
	HWND window = Game::Graphics::Instance().GetWindowHandle();
	if (window)
	{
		ShowWindow(window, SW_SHOW);
		SetForegroundWindow(window);
		SetActiveWindow(window);
		SetFocus(window);
	}
}

void SceneManager::JoinLoadThread()
{
	if (loadThread.joinable())
	{
		loadThread.join();
	}
}
