// SceneManager.h

#pragma once

#include "Common.h"
#include "Scene.h"

#include <atomic>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <type_traits>

class SceneManager
{
public:
	static SceneManager& Instance()
	{
		static SceneManager instance;
		return instance;
	}

	void Initialize();
	void Finalize();
	void Update();
	void Render();

	// 次のフレームでLoadingSceneへ切り替え、
	// 指定Sceneをロード用スレッド内で生成する。
	template<typename T>
	bool LoadScene(SceneMessage message = nullptr)
	{
		static_assert(
			std::is_base_of_v<Scene, T>,
			"T must inherit from Scene.");

		return RequestLoadScene(
			[message]() -> std::unique_ptr<Scene>
			{
				return std::make_unique<T>(message);
			});
	}

	// 旧呼び出しとの互換用。
	template<typename T>
	bool LoadSceneAsync(SceneMessage message = nullptr)
	{
		return LoadScene<T>(message);
	}

	bool IsLoading() const
	{
		return loading.load(std::memory_order_acquire);
	}

	float GetLoadProgress() const
	{
		return loadProgress;
	}

	std::string GetLastLoadError() const;

private:
	using SceneFactory =
		std::function<std::unique_ptr<Scene>()>;

	SceneManager() = default;
	~SceneManager();

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	struct LoadedScene
	{
		// Sceneを先に破棄し、その後PhysicsSceneContextを破棄する。
		std::unique_ptr<PhysicsSceneContext> physicsContext;
		std::unique_ptr<Scene> scene;
	};

	bool RequestLoadScene(SceneFactory sceneFactory);
	void BeginPendingLoad();
	bool StartLoadThread(SceneFactory sceneFactory);
	void UpdateLoadProgress();
	void ApplyLoadedScene();
	void JoinLoadThread();

	std::unique_ptr<Scene> currentScene;

	SceneFactory pendingSceneFactory;
	std::atomic_bool loadRequested = false;

	std::thread loadThread;
	std::atomic_bool loading = false;
	std::atomic_bool loadFinished = false;

	float loadProgress = 0.0f;

	mutable std::mutex loadMutex;
	std::unique_ptr<LoadedScene> loadedScene;
	std::exception_ptr loadException;
	std::string lastLoadError;
};
