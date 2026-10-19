#include "UI/BossBar.h"

#include <algorithm>
#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Entity.h"
#include "Rendering/Core/Graphics.h"

BossBar::BossBar() : GaugeHUD("Boss Bar", false)
{
	SetFrameTexture("Resources/UI/window.png");
	SetDissolveMask("Resources/UI/dissolve_animation.png");
	SetFillRect({0.10f, 0.33f}, {0.80f, 0.33f});
	SetColor(baseColor);
	SetFrameColor(Color(0.32f, 0.27f, 0.34f, 0.94f));
	SetDamageTrailEnabled(true);
	SetDissolveAmount(1.0f);
	SetValueImmediate(0.0f);
	SetValueSmoothingSpeed(5.0f);
	SetAffectedByPostProcess(false);
	SetActive(false);
}

void BossBar::SetColor(const Color& value)
{
	baseColor = value;
	GaugeHUD::SetColor(value);
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
		phaseColorElapsed = 0.0f;
		visibilityStart = GetDissolveAmount();
		visibilityTime = 0.0f;
		visibilityAnimating = true;
		fillTime = 0.0f;
		filling = true;
		SetValueImmediate(0.0f);
	}
	else if (!filling) SetValue(ratio);
	showing = true;
	SetActive(true);
}

void BossBar::Hide(const Entity* value)
{
	// 別のボスから遅れて届いたロスト通知で、現在のバーを消さない。
	if (value && value != boss) return;
	if (!showing) return;
	const auto bossOwner = bossLifetime.lock();
	if (bossOwner && boss->IsDead()) SetValue(0.0f);
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
		if (bossOwner && boss->IsDead()) SetValue(0.0f);
		Hide(boss);
	}

	const float dt = std::max(Game::Time::unscaledDeltaTime, 0.0f);
	if (showing)
	{
		const float lifeRatio = boss->GetMaxLife() > 0.0f
			? std::clamp(boss->GetLife() / boss->GetMaxLife(), 0.0f, 1.0f) : 0.0f;

		// Aracoreから持ってきた色変化の演出。HPが半分以下の時に赤くなる。
		phaseColorElapsed = lifeRatio <= 0.5f ? std::min(phaseColorElapsed + dt, 3.0f) : 0.0f;
		const float progress = phaseColorElapsed / 3.0f;
		const float smoothProgress = progress * progress * (3.0f - 2.0f * progress);
		const Color fillColor = Color::Lerp(baseColor, phaseColor, smoothProgress);
		GaugeHUD::SetColor(fillColor);

		if (filling)
		{
			fillTime += dt;
			const float t = std::clamp(fillTime / fillDuration, 0.0f, 1.0f);
			SetValueImmediate(lifeRatio * t * t * (3.0f - 2.0f * t));
			filling = t < 1.0f;
		}
		else SetValue(lifeRatio);
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
