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

	// 現在のSceneを進行させたまま、
	// 指定したSceneを別スレッドで生成する。
	template<typename T>
	bool LoadSceneAsync(SceneMessage message = nullptr)
	{
		static_assert(
			std::is_base_of_v<Scene, T>,
			"T must inherit from Scene.");

		return StartLoadSceneAsync(
			[message]() -> std::unique_ptr<Scene>
		{
			return std::make_unique<T>(message);
		});
	}

	bool IsLoading() const
	{
		return loading.load(std::memory_order_acquire);
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

	bool StartLoadSceneAsync(SceneFactory sceneFactory);

	void ApplyLoadedScene();
	void JoinLoadThread();

	std::unique_ptr<Scene> currentScene;

	std::thread loadThread;
	std::atomic_bool loading = false;
	std::atomic_bool loadFinished = false;

	mutable std::mutex loadMutex;
	std::unique_ptr<LoadedScene> loadedScene;
	std::exception_ptr loadException;
	std::string lastLoadError;
};