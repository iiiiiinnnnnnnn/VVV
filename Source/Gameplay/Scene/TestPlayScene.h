#pragma once

#include "Gameplay/Scene/Scene.h"
#include "Rendering/Effect/ParticleSystem.h"

class LocalPlayer;
class SpriteWidget;
class BossBar;

class TestPlayScene : public Scene
{
public:
	TestPlayScene();

	~TestPlayScene() override;

	void OnUpdate() override;
	void OnRender(RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	std::shared_ptr<LocalPlayer> player;
	std::shared_ptr<SpriteWidget> playerHudBack;
	std::shared_ptr<Widget> playerHudLife;
	std::shared_ptr<SpriteWidget> playerHudCircle;
	std::shared_ptr<BossBar> bossBar;
};
