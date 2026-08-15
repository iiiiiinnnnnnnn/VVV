#pragma once

#include "Gameplay/Stage/Stage.h"
#include "Gameplay/Stage/Component/StageLoader.h"

class Stage01 : public Stage
{
public:
	Stage01();
	void OnUpdate() override;
	void RenderEffects(const RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	void SpawnFogParticle();

	StageLoader* stageLoader = nullptr;
	std::unique_ptr<ParticleSystem> fogParticleSystem;
	float fogSpawnAccumulator = 0.0f;
	float fogSpawnRate = 12.0f;
	float fogAreaHalfWidth = 35.0f;
	float fogAreaHalfDepth = 35.0f;
	float fogMinHeight = 0.15f;
	float fogMaxHeight = 3.0f;
	float fogMinLifetime = 9.0f;
	float fogMaxLifetime = 15.0f;
	float fogFadeInDuration = 1.5f;
	float fogFadeOutDuration = 2.5f;
	float fogMinSize = 4.5f;
	float fogMaxSize = 9.0f;
	float fogDriftSpeed = 0.12f;
	Color fogColor = {0.72f, 0.78f, 0.82f, 0.025f};
	bool fogPrewarmed = false;
};
