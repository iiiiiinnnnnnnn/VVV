#include "Gameplay/Scene/PostProcessController.h"

void PostProcessController::RequestThreaten(float duration, float power, float attackRate, Easing::Type attackEasing, Easing::Type releaseEasing)
{
	threatenTimer = std::max(threatenTimer, duration);
	threatenDuration = std::max(threatenDuration, duration);
	threatenPower = std::max(threatenPower, power);
	threatenAttackRate = std::clamp(attackRate, 0.01f, 0.95f);
	threatenAttackEasing = attackEasing;
	threatenReleaseEasing = releaseEasing;
}

void PostProcessController::RequestDamagedVignette(float duration, float power, float attackRate, Easing::Type attackEasing, Easing::Type releaseEasing)
{
	damagedVigTimer = std::max(damagedVigTimer, duration);
	damagedVigDuration = std::max(damagedVigDuration, duration);
	damagedVigPower = std::max(damagedVigPower, power);
	damagedVigAttackRate = std::clamp(attackRate, 0.01f, 0.95f);
	damagedVigAttackEasing = attackEasing;
	damagedVigReleaseEasing = releaseEasing;
}

void PostProcessController::RequestJustDodge(float duration, float power)
{
	justDodgeTimer = std::max(justDodgeTimer, duration);
	justDodgeDuration = std::max(justDodgeDuration, duration);
	justDodgePower = std::max(justDodgePower, power);
}

void PostProcessController::RequestSkillStart(float duration, float power)
{
	skillStartTimer = std::max(skillStartTimer, duration);
	skillStartDuration = std::max(skillStartDuration, duration);
	skillStartPower = std::max(skillStartPower, power);
}

void PostProcessController::RequestSkillHit(float duration, float power)
{
	skillHitTimer = std::max(skillHitTimer, duration);
	skillHitDuration = std::max(skillHitDuration, duration);
	skillHitPower = std::max(skillHitPower, power);
}

void PostProcessController::Update()
{
	const float dt = Game::Time::unscaledDeltaTime;
	const auto updatePulse = [dt](float& timer, float& duration, float& power)
	{
		if (timer <= 0.0f) return;
		timer = std::max(timer - dt, 0.0f);
		if (timer <= 0.0f)
		{
			duration = 0.0f;
			power = 0.0f;
		}
	};

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

	updatePulse(justDodgeTimer, justDodgeDuration, justDodgePower);
	updatePulse(skillStartTimer, skillStartDuration, skillStartPower);
	updatePulse(skillHitTimer, skillHitDuration, skillHitPower);

	const float auraTarget = invincibilityAuraRequested ? 1.0f : 0.0f;
	const float auraResponse = invincibilityAuraRequested ? 14.0f : 6.0f;
	const float auraBlend = 1.0f - expf(-auraResponse * std::max(dt, 0.0f));
	invincibilityAuraIntensity +=
		(auraTarget - invincibilityAuraIntensity) * auraBlend;
	if (!invincibilityAuraRequested && invincibilityAuraIntensity < 0.001f)
		invincibilityAuraIntensity = 0.0f;
	invincibilityAuraRequested = false;
}

void PostProcessController::DrawGUI()
{
	// Threaten
	{
		static float duration = 5.0f;
		static float power = 3.0f;
		static float attackRate = 0.15f;
		static Easing::Type attackEasing = Easing::Type::InSine;
		static Easing::Type releaseEasing = Easing::Type::OutCubic;
		ImGui::PushID("Threaten");
		ImGui::DragFloat("duration", &duration, 0.1f, 0.1f, 10.0f);
		ImGui::DragFloat("power", &power, 0.1f, 0.1f, 10.0f);
		ImGui::DragFloat("attackRate", &attackRate, 0.01f, 0.01f, 0.95f);
		ImGui::DragInt(("Attack Easing: " + std::string(magic_enum::enum_name(attackEasing))).c_str(), (int*)&attackEasing, 1.0f, 0, static_cast<int>(Easing::Type::Count) - 1);
		ImGui::DragInt(("Release Easing: " + std::string(magic_enum::enum_name(releaseEasing))).c_str(), (int*)&releaseEasing, 1.0f, 0, static_cast<int>(Easing::Type::Count) - 1);
		if (ImGui::Button("TEST_IKAKU", ImVec2(-FLT_MIN, 30.0f)))
		{
			RequestThreaten(duration, power, attackRate, attackEasing, releaseEasing);
		}
		ImGui::PopID();
	}

	// Damaged Vignette
	{
		static float duration = 1.0f;
		static float power = 0.5f;
		static float attackRate = 0.05f;
		static Easing::Type attackEasing = Easing::Type::Linear;
		static Easing::Type releaseEasing = Easing::Type::InOutQuad;
		ImGui::PushID("Damaged Vignette");
		ImGui::DragFloat("duration", &duration, 0.1f, 0.1f, 10.0f);
		ImGui::DragFloat("power", &power, 0.1f, 0.1f, 10.0f);
		ImGui::DragFloat("attackRate", &attackRate, 0.01f, 0.01f, 0.95f);
		ImGui::DragInt(("Attack Easing: " + std::string(magic_enum::enum_name(attackEasing))).c_str(), (int*)&attackEasing, 1.0f, 0, static_cast<int>(Easing::Type::Count) - 1);
		ImGui::DragInt(("Release Easing: " + std::string(magic_enum::enum_name(releaseEasing))).c_str(), (int*)&releaseEasing, 1.0f, 0, static_cast<int>(Easing::Type::Count) - 1);
		if (ImGui::Button("TEST_DAMAGE_VIG", ImVec2(-FLT_MIN, 30.0f)))
		{
			RequestDamagedVignette(duration, power, attackRate, attackEasing, releaseEasing);
		}
		ImGui::PopID();
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
		postProcess.AddRuntimeVignette(damagedVig, {0.7f, 0, 0, 1});
	}

	const float justDodge = GetIntensity(justDodgeTimer, justDodgeDuration,
		justDodgePower, 0.12f, Easing::Type::OutQuad, Easing::Type::OutCubic);
	if (justDodge > 0.001f)
	{
		postProcess.AddRuntimeRadialBlur(justDodge * 0.22f);
		postProcess.AddRuntimeChromaticAberration(justDodge * 1.05f);
		postProcess.AddRuntimeVignette(justDodge * 0.32f, {0.05f, 0.48f, 0.95f, 1.0f});
	}

	const float skillStart = GetIntensity(skillStartTimer, skillStartDuration,
		skillStartPower, 0.10f, Easing::Type::OutQuad, Easing::Type::OutCubic);
	if (skillStart > 0.001f)
	{
		postProcess.AddRuntimeRadialBlur(skillStart * 0.18f);
		postProcess.AddRuntimeChromaticAberration(skillStart * 0.62f);
		postProcess.AddRuntimeVignette(skillStart * 0.30f, {0.2f, 0.62f, 1.0f, 1.0f});
	}

	if (invincibilityAuraIntensity > 0.001f)
	{
		// 無敵期間全体で、青白い縁と色分離を明確に残す。
		postProcess.AddRuntimeChromaticAberration(
			invincibilityAuraIntensity * 0.22f);
		postProcess.AddRuntimeVignette(
			invincibilityAuraIntensity * 0.26f, {0.08f, 0.5f, 1.0f, 1.0f});
	}

	const float skillHit = GetIntensity(skillHitTimer, skillHitDuration,
		skillHitPower, 0.08f, Easing::Type::OutQuad, Easing::Type::OutCubic);
	if (skillHit > 0.001f)
	{
		postProcess.AddRuntimeRadialBlur(skillHit * 0.38f);
		postProcess.AddRuntimeChromaticAberration(skillHit * 1.15f);
		postProcess.AddRuntimeVignette(skillHit * 0.34f, {0.9f, 0.65f, 0.18f, 1.0f});
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
