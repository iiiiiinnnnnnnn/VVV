// GameStartScene.h
#pragma once

#include "Gameplay/Scene/Scene.h"
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/CacheSettings.h"
#include <map>
#endif

class SpriteWidget;
class ColorWidget;
class TextWidget;

class GameStartScene : public Scene
{
public:
	GameStartScene();
	~GameStartScene() override;

	void OnUpdate() override;
	void OnDrawGUI() override;
	MouseCursorMode GetMouseCursorMode() const override { return MouseCursorMode::VisibleFree; }

private:
	void ConfigureWindow();
	void UpdateHeaderParallax();
	void UpdateLauncherWidgetLayout();
	bool DrawMenuButton(const char* label, const ImVec4& normal,
		const ImVec4& hovered, const ImVec4& active, const ImVec2& size);

	bool loadRequested = false;
	bool windowConfigured = false;
	std::shared_ptr<SpriteWidget> headerWidget;
	std::shared_ptr<TextWidget> headerText;
	Vector2 headerParallaxOffset = Vector2::Zero;
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	struct ResourceTreeNode
	{
		std::string name;
		std::string path;
		bool file = false;
		std::map<std::string, ResourceTreeNode> children;
	};

	void DrawTreeLabel(const ResourceTreeNode& node, int depth,
		const std::vector<bool>& guides, bool last);
	void AddResourceTreePath(ResourceTreeNode& root, const std::string& path);
	void DrawCacheTreeNode(ResourceTreeNode& node, int depth,
		const std::vector<bool>& guides, bool last);
	void DrawResourceSettings();
	void ReloadCacheList();
	bool showResourceSettings = false;
	CacheSettings cacheSettings;
	std::vector<std::string> cachePaths;
	std::string cacheMessage;
#endif
};
