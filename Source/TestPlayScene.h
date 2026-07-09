// TestPlayScene.h

#pragma once

#include "Scene.h"
#include "ParticleSystem.h"

class TestPlayScene : public Scene
{
public:
	TestPlayScene(SceneMessage message = nullptr);

	~TestPlayScene() override = default;

	void OnUpdate() override;
	void OnRender(RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	std::unique_ptr<ParticleSystem> particleSystem;
};