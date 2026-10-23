#pragma once

#include "Gameplay/Scene/Scene.h"
#include "UI/TitleHUD.h"

class Actor;
class DummyPlayer;

class TitleScene : public Scene
{
public:
	TitleScene();
	~TitleScene() override;

protected:
	void OnInit() override;
	void OnUninit() override;
	void OnUpdate() override;
	void OnDrawGUI() override;
	bool UsesGameDebugGUI() const override;
	MouseCursorMode GetMouseCursorMode() const override { return MouseCursorMode::VisibleFree; }

private:
	void OnMenuSelected(int index);
	void CreateTitleWorld();
	void RequestExit();
	std::shared_ptr<DummyPlayer> dummyPlayer;
	std::shared_ptr<TitleHUD> titleHud;
	bool loadRequested = false;
	bool exitRequested = false;
	TitleHUD::MenuPage prevMenuPage = TitleHUD::MenuPage::Prompt; // 監視用
};