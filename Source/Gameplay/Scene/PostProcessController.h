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

	// ジャスト回避・SP攻撃の短い画面演出。タイマーは非スケール時間で進む。
	void RequestJustDodge(float duration = 0.28f, float power = 1.0f);
	void RequestSkillStart(float duration = 0.38f, float power = 1.0f);
	void RequestSkillHit(float duration = 0.30f, float power = 1.0f);
	// 無敵中は毎フレーム呼び出す。呼び出しが止まると自然にフェードする。
	void RequestInvincibilityAura() { invincibilityAuraRequested = true; }
	// 死亡中は毎フレーム呼び出す。呼び出しが止まると次のフレームで解除する。
	void RequestDeathVignette(float progress)
	{
		deathVignetteProgress = std::clamp(progress, 0.0f, 1.0f);
		deathVignetteRequested = true;
	}

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

	float justDodgeTimer = 0.0f;
	float justDodgeDuration = 0.0f;
	float justDodgePower = 0.0f;
	float skillStartTimer = 0.0f;
	float skillStartDuration = 0.0f;
	float skillStartPower = 0.0f;
	float skillHitTimer = 0.0f;
	float skillHitDuration = 0.0f;
	float skillHitPower = 0.0f;
	float invincibilityAuraIntensity = 0.0f;
	bool invincibilityAuraRequested = false;
	float deathVignetteProgress = 0.0f;
	bool deathVignetteRequested = false;
};
