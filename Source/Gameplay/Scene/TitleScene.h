#pragma once

#include "Gameplay/Scene/Scene.h"
#include <vector>

class Actor;
class WidgetGroup;
class TitleLogoWidget;
class MenuButtonWidget;

class TitleScene : public Scene
{
public:
	TitleScene();
	~TitleScene() override;

protected:
	void OnInit() override;
	void OnUpdate() override;
	void OnDrawGUI() override;
	bool UsesGameDebugGUI() const override;
	MouseCursorMode GetMouseCursorMode() const override { return MouseCursorMode::VisibleFree; }

private:
	enum class MenuAction
	{
		Play,
		Control,
		Exit,
		VmdlEditor,
		VstgEditor,
		Resources
	};
	enum class MenuPage
	{
		Main,
		Resources
	};

	void OnMenuSelected(MenuAction action);
	void CreateTitleUI();
	void SetMenuPage(MenuPage page);
	void SetButtonsInteractable(bool interactable);
	void DrawResources();
	void CreateTitleWorld();
	void RequestExit();
	std::shared_ptr<WidgetGroup> titleHud;
	std::shared_ptr<WidgetGroup> mainHud;
	std::shared_ptr<TitleLogoWidget> titleLogo;
	std::vector<std::shared_ptr<MenuButtonWidget>> menuButtons;
	MenuPage menuPage = MenuPage::Main;
	bool loadRequested = false;
	bool exitRequested = false;
};