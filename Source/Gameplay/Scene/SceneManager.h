#pragma once
#include <memory>
#include <string>

#include "Core/Foundation/Common.h"
#include "Gameplay/Scene/Scene.h"
#include "Physics/Core/PhysicsManager.h"

#include <atomic>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

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
	void ApplyPendingChanges();
	void Update();
	void Render();

	template <typename T, typename... Args> bool LoadScene(Args&&... args)
	{
		static_assert(std::is_base_of_v<Scene, T>, "T must inherit from Scene.");
		auto arguments = std::make_tuple(std::forward<Args>(args)...);

		return RequestLoadScene(
			[arguments = std::move(arguments)]() mutable -> std::unique_ptr<Scene> {
				return std::apply(
					[](auto&&... values) -> std::unique_ptr<Scene> {
						return std::make_unique<T>(std::forward<decltype(values)>(values)...);
					},
					std::move(arguments));
			});
	}

	template <typename T> bool LoadSceneAsync() { return LoadScene<T>(); }

	bool IsLoading() const { return loading.load(std::memory_order_acquire); }

	float GetLoadProgress() const { return loadProgress; }

	Scene* GetCurrentScene() { return currentScene.get(); }

	const Scene* GetCurrentScene() const { return currentScene.get(); }

	CameraController* GetActiveCameraController() const
	{
		if (!currentScene)
		{
			return nullptr;
		}

		return currentScene->GetActiveCameraController();
	}

	std::string GetLastLoadError() const;

  private:
	using SceneFactory = std::function<std::unique_ptr<Scene>()>;

	SceneManager() = default;
	~SceneManager();

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	struct LoadedScene
	{
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
