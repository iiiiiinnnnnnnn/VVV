// PostProcessController.h

#pragma once

#include <algorithm>
#include "Easing.h"
#include "GameTime.h"
#include "GamePostProcess.h"

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
		threatenTimer = 0.0f;
		threatenDuration = 0.0f;
		threatenPower = 0.0f;
	}

	void RequestThreaten(
		float duration,
		float power = 1.0f,
		float attackRate = 0.25f,
		Easing::Type attackEasing = Easing::Type::InSine,
		Easing::Type releaseEasing = Easing::Type::OutCubic)
	{
		threatenTimer = max(threatenTimer, duration);
		threatenDuration = max(threatenDuration, duration);
		threatenPower = max(threatenPower, power);

		threatenAttackRate = std::clamp(attackRate, 0.01f, 0.95f);
		threatenAttackEasing = attackEasing;
		threatenReleaseEasing = releaseEasing;
	}

	void Update()
	{
		const float dt = Game::Time::unscaledDeltaTime;

		if (threatenTimer > 0.0f)
		{
			threatenTimer -= dt;

			if (threatenTimer <= 0.0f)
			{
				threatenTimer = 0.0f;
				threatenDuration = 0.0f;
				threatenPower = 0.0f;
			}
		}
	}

	void DrawGUI()
	{
		if (ImGui::Button("TEST_IKAKU", ImVec2(-FLT_MIN, 30.0f)))
		{
			PostProcessController::Instance().RequestThreaten(
				5.0f,
				3.0f,
				0.15f,
				Easing::Type::InSine,
				Easing::Type::OutCubic);
		}
	}

	void ApplyTo(Game::PostProcess& postProcess) const
	{
		const float threaten = GetThreatenIntensity();

		if (threaten > 0.001f)
		{
			postProcess.AddRuntimeRadialBlur(threaten);
		}
	}

private:
	PostProcessController() = default;

	// エンベロープを計算して威嚇演出の強度を返す
	float GetThreatenIntensity() const
	{
		if (threatenDuration <= 0.0f)
		{
			return 0.0f;
		}

		const float remainRate = std::clamp(
			threatenTimer / threatenDuration,
			0.0f,
			1.0f);

		const float progress = 1.0f - remainRate;

		float envelope = 0.0f;

		if (progress < threatenAttackRate)
		{
			const float attackT = progress / threatenAttackRate;
			envelope = Easing::Evaluate(attackT, threatenAttackEasing);
		}
		else
		{
			const float releaseT = (progress - threatenAttackRate) / (1.0f - threatenAttackRate);
			envelope = 1.0f - Easing::Evaluate(releaseT, threatenReleaseEasing);
		}

		return threatenPower * envelope;
	}

private:
	float threatenTimer = 0.0f;
	float threatenDuration = 0.0f;
	float threatenPower = 0.0f;

	float threatenAttackRate = 0.25f;
	Easing::Type threatenAttackEasing = Easing::Type::InSine;
	Easing::Type threatenReleaseEasing = Easing::Type::OutCubic;
};