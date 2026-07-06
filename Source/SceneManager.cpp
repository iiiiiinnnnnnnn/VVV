// SceneManager.cpp

#include "SceneManager.h"

#include "GameTime.h"
#include "LoadingScene.h"
#include "PhysicsManager.h"

#include <stdexcept>

namespace
{
	class ThreadSceneContextScope
	{
	public:
		explicit ThreadSceneContextScope(
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
	currentScene = std::make_unique<LoadingScene>();
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

void SceneManager::Update()
{
	// 前フレームまでに完了したSceneを適用する。
	ApplyLoadedScene();

	// Scene内からLoadSceneされた場合でも、
	// 呼び出し元SceneのUpdate終了後となる次フレームに開始する。
	BeginPendingLoad();

	UpdateLoadProgress();

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

	// 遷移元Sceneを安全なタイミングで破棄し、
	// ロード中表示専用Sceneへ切り替える。
	currentScene.reset();
	currentScene = std::make_unique<LoadingScene>();

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

			try
			{
				result =
					std::make_unique<LoadedScene>();

				result->physicsContext =
					PhysicsManager::Instance().
					CreateSceneContext();

				{
					ThreadSceneContextScope contextScope(
						result->physicsContext.get());

					// ロード用スレッド内では通常どおり同期的に
					// Sceneのコンストラクタを最後まで実行する。
					result->scene = sceneFactory();
				}

				if (!result->scene)
				{
					throw std::runtime_error(
						"Failed to create scene.");
				}
			}
			catch (...)
			{
				exception =
					std::current_exception();

				result.reset();
			}

			{
				std::lock_guard<std::mutex> lock(
					loadMutex);

				loadedScene = std::move(result);
				loadException = exception;
			}

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

	// Scene側へ進捗報告を書かせないため、表示用の進捗値を作る。
	// 実際の完了判定はloadFinishedで行う。
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

	PhysicsManager::Instance().
		SetCurrentSceneContext(
			std::move(result->physicsContext));

	currentScene =
		std::move(result->scene);
}

void SceneManager::JoinLoadThread()
{
	if (loadThread.joinable())
	{
		loadThread.join();
	}
}
