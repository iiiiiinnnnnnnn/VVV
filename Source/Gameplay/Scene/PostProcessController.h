// PostProcessController.h

#pragma once

#include <algorithm>
#include "Core/Foundation/Easing.h"
#include "Application/Time/GameTime.h"
#include "Rendering/Shader/GamePostProcess.h"
#include "imgui.h"

class PostProcessController
{
public:
	static PostProcessController& Instance()
	{
		static PostProcessController instance;
		return instance;
	}

	void Reset()
	{
		*this = PostProcessController{};
	}

	void RequestThreaten(
		float duration,
		float power = 1.0f,
		float attackRate = 0.25f,
		Easing::Type attackEasing = Easing::Type::InSine,
		Easing::Type releaseEasing = Easing::Type::OutCubic);

	void RequestDamagedVignette(
		float duration,
		float power = 1.0f,
		float attackRate = 0.25f,
		Easing::Type attackEasing = Easing::Type::InSine,
		Easing::Type releaseEasing = Easing::Type::OutCubic);

	void Update();

	void DrawGUI();

	void ApplyTo(Game::PostProcess& postProcess) const;

private:
	PostProcessController() = default;

	// エンベロープを計算して威嚇演出の強度を返す
	float GetIntensity(float timer, float duration, float power, float attackRate, Easing::Type attack, Easing::Type release) const;

private:
	float threatenTimer = 0.0f;
	float threatenDuration = 0.0f;
	float threatenPower = 0.0f;
	float threatenAttackRate = 0.25f;
	Easing::Type threatenAttackEasing = Easing::Type::InSine;
	Easing::Type threatenReleaseEasing = Easing::Type::OutCubic;

	float damagedVigTimer = 0.0f;
	float damagedVigDuration = 0.0f;
	float damagedVigPower = 0.0f;
	float damagedVigAttackRate = 0.25f;
	Easing::Type damagedVigAttackEasing = Easing::Type::InSine;
	Easing::Type damagedVigReleaseEasing = Easing::Type::OutCubic;
};