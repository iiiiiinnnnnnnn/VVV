// Stage01.h

#pragma once

#include <utility>

#include "Gameplay/Stage/Stage.h"
#include "Gameplay/Stage/Component/StageLoader.h"

class Stage01 : public Stage
{
public:
	Stage01();
	void OnUpdate() override;
	void RenderEffects(const RenderContext& rc) override;
	void OnDrawGUI() override;

	void SetFogParticleSystem(std::unique_ptr<ParticleSystem> system)
	{
		fogParticleSystem = std::move(system);
	}
	ParticleSystem* GetFogParticleSystem() const { return fogParticleSystem.get(); }

	void SetFogSpawnAccumulator(float value) { fogSpawnAccumulator = value; }
	float GetFogSpawnAccumulator() const { return fogSpawnAccumulator; }
	void SetFogSpawnRate(float value) { fogSpawnRate = value; }
	float GetFogSpawnRate() const { return fogSpawnRate; }
	void SetFogAreaHalfWidth(float value) { fogAreaHalfWidth = value; }
	float GetFogAreaHalfWidth() const { return fogAreaHalfWidth; }
	void SetFogAreaHalfDepth(float value) { fogAreaHalfDepth = value; }
	float GetFogAreaHalfDepth() const { return fogAreaHalfDepth; }

	void SetFogMinHeight(float value) { fogMinHeight = value; }
	float GetFogMinHeight() const { return fogMinHeight; }
	void SetFogMaxHeight(float value) { fogMaxHeight = value; }
	float GetFogMaxHeight() const { return fogMaxHeight; }
	void SetFogHeightRange(float minValue, float maxValue)
	{
		fogMinHeight = minValue;
		fogMaxHeight = maxValue;
	}

	void SetFogMinLifetime(float value) { fogMinLifetime = value; }
	float GetFogMinLifetime() const { return fogMinLifetime; }
	void SetFogMaxLifetime(float value) { fogMaxLifetime = value; }
	float GetFogMaxLifetime() const { return fogMaxLifetime; }
	void SetFogLifetimeRange(float minValue, float maxValue)
	{
		fogMinLifetime = minValue;
		fogMaxLifetime = maxValue;
	}

	void SetFogFadeInDuration(float value) { fogFadeInDuration = value; }
	float GetFogFadeInDuration() const { return fogFadeInDuration; }
	void SetFogFadeOutDuration(float value) { fogFadeOutDuration = value; }
	float GetFogFadeOutDuration() const { return fogFadeOutDuration; }
	void SetFogFadeDurations(float fadeIn, float fadeOut)
	{
		fogFadeInDuration = fadeIn;
		fogFadeOutDuration = fadeOut;
	}

	void SetFogMinSize(float value) { fogMinSize = value; }
	float GetFogMinSize() const { return fogMinSize; }
	void SetFogMaxSize(float value) { fogMaxSize = value; }
	float GetFogMaxSize() const { return fogMaxSize; }
	void SetFogSizeRange(float minValue, float maxValue)
	{
		fogMinSize = minValue;
		fogMaxSize = maxValue;
	}

	void SetFogDriftSpeed(float value) { fogDriftSpeed = value; }
	float GetFogDriftSpeed() const { return fogDriftSpeed; }
	void SetFogColor(const Color& value) { fogColor = value; }
	const Color& GetFogColor() const { return fogColor; }
	void SetFogPrewarmed(bool value) { fogPrewarmed = value; }
	bool IsFogPrewarmed() const { return fogPrewarmed; }

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
