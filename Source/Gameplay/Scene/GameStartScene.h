// GameStartScene.h
#pragma once

#include "Gameplay/Scene/Scene.h"
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/CacheSettings.h"
#include "Audio/SoundTrackRegistry.h"
#endif

class SpriteWidget;

class GameStartScene : public Scene
{
public:
	GameStartScene();
	~GameStartScene() override;

	void OnUpdate() override;
	void OnDrawGUI() override;

private:
	void ConfigureWindow();
	void UpdateHeaderParallax();
	void UpdateLauncherWidgetLayout();

	bool loadRequested = false;
	bool windowConfigured = false;
	std::shared_ptr<SpriteWidget> headerWidget;
	Vector2 headerParallaxOffset = Vector2::Zero;
	float headerLayoutHeight = 176.0f;
	float headerTop = 34.0f;
	float headerVignetteStrength = 1.0f;
	float headerVignetteRange = 0.32f;
	float headerVignetteSoftness = 0.32f;
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	void DrawCacheManager();
	void ReloadCacheList();
	void DrawSoundManager();
	void ReloadSoundTracks();
	bool showCacheManager = false;
	bool showSoundManager = false;
	CacheSettings cacheSettings;
	std::vector<std::string> cachePaths;
	std::string cacheMessage;
	SoundTrackRegistry soundTracks;
	std::string soundMessage;
#endif
};
