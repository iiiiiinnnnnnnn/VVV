// PostProcessController.cpp

#include "PostProcessController.h"

void PostProcessController::RequestThreaten(float duration, float power, float attackRate, Easing::Type attackEasing, Easing::Type releaseEasing)
{
	threatenTimer = max(threatenTimer, duration);
	threatenDuration = max(threatenDuration, duration);
	threatenPower = max(threatenPower, power);
	threatenAttackRate = std::clamp(attackRate, 0.01f, 0.95f);
	threatenAttackEasing = attackEasing;
	threatenReleaseEasing = releaseEasing;
}

void PostProcessController::RequestDamagedVignette(float duration, float power, float attackRate, Easing::Type attackEasing, Easing::Type releaseEasing)
{
	damagedVigTimer = max(damagedVigTimer, duration);
	damagedVigDuration = max(damagedVigDuration, duration);
	damagedVigPower = max(damagedVigPower, power);
	damagedVigAttackRate = std::clamp(attackRate, 0.01f, 0.95f);
	damagedVigAttackEasing = attackEasing;
	damagedVigReleaseEasing = releaseEasing;
}

void PostProcessController::Update()
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

	if (damagedVigTimer > 0.0f)
	{
		damagedVigTimer -= dt;
		if (damagedVigTimer <= 0.0f)
		{
			damagedVigTimer = 0.0f;
			damagedVigDuration = 0.0f;
			damagedVigPower = 0.0f;
		}
	}
}

void PostProcessController::DrawGUI()
{
	if (ImGui::Button("TEST_IKAKU", ImVec2(-FLT_MIN, 30.0f)))
	{
		RequestThreaten(
			5.0f,
			3.0f,
			0.15f,
			Easing::Type::InSine,
			Easing::Type::OutCubic);
	}
	if (ImGui::Button("TEST_DAMAGE_VIG", ImVec2(-FLT_MIN, 30.0f)))
	{
		RequestDamagedVignette(
			5.0f,
			1.0f,
			0.15f,
			Easing::Type::InSine,
			Easing::Type::OutCubic);
	}
}

void PostProcessController::ApplyTo(Game::PostProcess& postProcess) const
{
	const float threaten = GetIntensity(
		threatenTimer,
		threatenDuration,
		threatenPower,
		threatenAttackRate,
		threatenAttackEasing,
		threatenReleaseEasing);
	if (threaten > 0.001f)
	{
		postProcess.AddRuntimeRadialBlur(threaten);
	}

	const float damagedVig = GetIntensity(
		damagedVigTimer,
		damagedVigDuration,
		damagedVigPower,
		damagedVigAttackRate,
		damagedVigAttackEasing,
		damagedVigReleaseEasing);
	if (damagedVig > 0.001f)
	{
		postProcess.AddRuntimeVignette(damagedVig, {0.7f, 0, 0, 0});
	}
}

float PostProcessController::GetIntensity(float timer, float duration, float power, float attackRate, Easing::Type attack, Easing::Type release) const
{
	if (duration <= 0.0f)
	{
		return 0.0f;
	}

	const float remainRate = std::clamp(
		timer / duration,
		0.0f,
		1.0f);

	const float progress = 1.0f - remainRate;

	float envelope = 0.0f;

	if (progress < attackRate)
	{
		const float attackT = progress / attackRate;
		envelope = Easing::Evaluate(attackT, attack);
	}
	else
	{
		const float releaseT = (progress - attackRate) / (1.0f - attackRate);
		envelope = 1.0f - Easing::Evaluate(releaseT, release);
	}

	return power * envelope;
}
