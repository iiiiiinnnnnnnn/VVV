#include "UI/BossBar.h"

#include <algorithm>
#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Entity.h"
#include "Rendering/Core/Graphics.h"

BossBar::BossBar() : GaugeHUD("Boss Bar")
{
	SetFrameTexture("Resources/UI/window.png");
	SetDissolveMask("Resources/UI/dissolve_animation.png");
	SetFillRect({0.10f, 0.33f}, {0.80f, 0.33f});
	SetColors(Color(0.04f, 0.48f, 0.95f, 0.98f),
		Color(0.04f, 0.48f, 0.95f, 0.98f), Color(0.32f, 0.27f, 0.34f, 0.94f));
	SetDamageTrailEnabled(true);
	SetDissolveAmount(1.0f);
	SetTargetValue(0.0f);
	SnapToTarget();
	SetSmoothingSpeed(5.0f);
	SetAffectedByPostProcess(false);
	SetActive(false);
}

void BossBar::Show(Entity* value)
{
	if (!value || value->IsPendingDestroy() || value->IsDead()) return;

	const bool changedBoss = boss != value;
	boss = value;
	bossLifetime = value->weak_from_this();
	const float ratio = boss->GetMaxLife() > 0.0f
		? std::clamp(boss->GetLife() / boss->GetMaxLife(), 0.0f, 1.0f)
		: 0.0f;
	if (changedBoss || !showing)
	{
		visibilityStart = GetDissolveAmount();
		visibilityTime = 0.0f;
		visibilityAnimating = true;
		fillTime = 0.0f;
		filling = true;
		SetTargetValue(0.0f);
		SnapToTarget();
	}
	else if (!filling) SetTargetValue(ratio);
	showing = true;
	SetActive(true);
}

void BossBar::Hide(const Entity* value)
{
	// 別のボスから遅れて届いたロスト通知で、現在のバーを消さない。
	if (value && value != boss) return;
	if (!showing) return;
	const auto bossOwner = bossLifetime.lock();
	if (bossOwner && boss->IsDead()) SetTargetValue(0.0f);
	boss = nullptr;
	bossLifetime.reset();
	showing = false;
	filling = false;
	visibilityStart = GetDissolveAmount();
	visibilityTime = 0.0f;
	visibilityAnimating = true;
}

void BossBar::OnUpdate()
{
	const auto bossOwner = bossLifetime.lock();
	if (showing && (!bossOwner || boss->IsPendingDestroy() || !boss->IsActive() || boss->IsDead()))
	{
		if (bossOwner && boss->IsDead()) SetTargetValue(0.0f);
		Hide(boss);
	}

	const float dt = std::max(Game::Time::unscaledDeltaTime, 0.0f);
	if (showing)
	{
		const float lifeRatio = boss->GetMaxLife() > 0.0f
			? std::clamp(boss->GetLife() / boss->GetMaxLife(), 0.0f, 1.0f) : 0.0f;
		if (filling)
		{
			fillTime += dt;
			const float t = std::clamp(fillTime / fillDuration, 0.0f, 1.0f);
			SetTargetValue(lifeRatio * t * t * (3.0f - 2.0f * t));
			SnapToTarget();
			filling = t < 1.0f;
		}
		else SetTargetValue(lifeRatio);
	}
	GaugeHUD::OnUpdate();
	if (visibilityAnimating)
	{
		visibilityTime += dt;
		const float duration = showing ? showDuration : hideDuration;
		const float t = std::clamp(visibilityTime / duration, 0.0f, 1.0f);
		const float target = showing ? 0.0f : 1.0f;
		SetDissolveAmount(visibilityStart + (target - visibilityStart) * t * t * (3.0f - 2.0f * t));
		visibilityAnimating = t < 1.0f;
		if (!showing && !visibilityAnimating) SetActive(false);
	}

	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	const float hudScale = std::clamp(
		std::min(screenWidth / 1920.0f, screenHeight / 1080.0f), 0.68f, 1.35f) *
		0.78f;
	rect.position = {screenWidth * 0.5f, 22.0f * hudScale};
	rect.anchor = {0.5f, 0.0f};
	rect.size = {540.0f * hudScale, 58.0f * hudScale};
}

void BossBar::OnDrawGUI()
{
	GaugeHUD::OnDrawGUI();
	ImGui::SliderFloat((const char*)u8"出現時間", &showDuration, 0.05f, 2.0f);
	ImGui::SliderFloat((const char*)u8"消去時間", &hideDuration, 0.05f, 2.0f);
	ImGui::SliderFloat((const char*)u8"HP充填時間", &fillDuration, 0.05f, 3.0f);
}
