// TestPlayScene.h

#pragma once

#include "Gameplay/Scene/Scene.h"
#include "Rendering/Effect/ParticleSystem.h"

class TestPlayScene : public Scene
{
public:
	TestPlayScene(SceneMessage message = nullptr);

	~TestPlayScene() override;

	void OnUpdate() override;
	void OnRender(RenderContext& rc) override;
	void OnDrawGUI() override;

private:

};
