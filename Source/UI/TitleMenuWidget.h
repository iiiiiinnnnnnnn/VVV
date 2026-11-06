#pragma once

#include "UI/WidgetGroup.h"
#include <functional>
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/CacheSettings.h"
#include <map>
#endif

class TitleLogoWidget;
class SpriteWidget;
class PressAnyButtonWidget;
class ButtonWidget;

class TitleMenuWidget final : public WidgetGroup
{
public:
	void OnAwake() override;
	TitleMenuWidget(std::function<void(int)> onSelected);
	void SetInputEnabled(bool enabled) { inputEnabled = enabled; }
	bool IsResourcesPage() const { return menuPage == MenuPage::Resources; }
	void DrawResources();
	enum class MenuPage
	{
		Prompt,
		Main,
		Resources
	};
	const MenuPage& GetMenuPage() { return menuPage; };

protected:
	void OnUpdate() override;

private:
	void OnMenuSelected(int index);
	void UpdateLayout();
	void UpdateMenu();
	void SetMenuPage(MenuPage page);
	std::function<void(int)> onSelected;
	std::shared_ptr<WidgetGroup> mainHud;
	std::shared_ptr<WidgetGroup> toolsHud;
	std::shared_ptr<SpriteWidget> dimmer;
	std::shared_ptr<SpriteWidget> fadeOverlay;
	std::shared_ptr<TitleLogoWidget> titleLogo;
	std::shared_ptr<PressAnyButtonWidget> pressAnyButton;
	std::vector<std::shared_ptr<ButtonWidget>> menuButtons;
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
	CacheSettings cacheSettings;
	std::vector<std::string> cachePaths;
	std::string cacheMessage;
#endif
	MenuPage menuPage = MenuPage::Prompt;
	float fadeAlpha = 1.0f;
	float menuTransition = 0.0f;
	bool waitForCancelRelease = false;
	bool inputEnabled = true;
};
