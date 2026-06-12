// SceneManager.cpp

#include "SceneManager.h"

#include "PhysicsManager.h"
#include "LoadingScene.h"

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
}

void SceneManager::Finalize()
{
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

	// 現在のPhysXシーンが有効な間に、
	// 現在のScene内の物理オブジェクトを破棄する。
	currentScene.reset();

	// LoadedScene内のSceneを先に破棄し、
	// その後ロード用PhysicsSceneContextを破棄する。
	sceneWaitingForDestruction.reset();
}

void SceneManager::Update()
{
	ApplyLoadedScene();

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

bool SceneManager::StartLoadSceneAsync(
	SceneFactory sceneFactory)
{
	bool expected = false;

	if (!loading.compare_exchange_strong(
		expected,
		true,
		std::memory_order_acq_rel))
	{
		// すでにロード中。
		return false;
	}

	// 前回のロードスレッドが残っていれば終了を確定させる。
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

				// 次のScene専用のPhysXシーンを生成する。
				result->physicsContext =
					PhysicsManager::Instance().
					CreateSceneContext();

				{
					// このスレッド内で生成される物理オブジェクトを
					// 次のScene専用のPhysXシーンへ登録する。
					ThreadSceneContextScope contextScope(
						result->physicsContext.get());

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

		loading.store(
			false,
			std::memory_order_release);

		loadFinished.store(
			false,
			std::memory_order_release);

		return false;
	}

	return true;
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
			"[SceneManager] LoadSceneAsync failed: " +
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

	// 旧Sceneのデストラクタが旧PhysXシーンを
	// 参照できる状態で破棄する。
	currentScene.reset();

	// 新しいPhysXシーンへ切り替える。
	PhysicsManager::Instance().
		SetCurrentSceneContext(
		std::move(result->physicsContext));

	// 新しいSceneへ切り替える。
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