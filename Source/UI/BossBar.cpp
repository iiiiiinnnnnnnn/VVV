#include "UI/BossBar.h"

#include <algorithm>
#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Entity.h"
#include "Rendering/Core/Graphics.h"

BossBar::BossBar() : GaugeHUD("Boss Bar", false)
{
	rect.position = {Game::Graphics::ScreenWidth / 2.0f, 37.800f};
	rect.size = {819.000f, 57.720f};
	rect.anchor = {0.5f, 0.0f};
	SetFrameTexture("Resources/UI/window.png");
	SetDissolveMask("Resources/UI/dissolve_animation.png");
	SetFillRect({0.15f, 0.25f}, {0.75f, 0.52f});
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
		phaseColorElapsed.Reset();
		visibilityStart = GetDissolveAmount();
		visibilityTime.Reset();
		visibilityAnimating = true;
		fillTime.Reset();
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
	visibilityTime.Reset();
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
		if (lifeRatio <= 0.5f) phaseColorElapsed.Update(dt);
		else phaseColorElapsed.Reset();
		const float progress = 1.0f - phaseColorElapsed.GetRemaining() / phaseColorElapsed.GetDuration();
		const float smoothProgress = progress * progress * (3.0f - 2.0f * progress);
		const Color fillColor = Color::Lerp(baseColor, phaseColor, smoothProgress);
		GaugeHUD::SetColor(fillColor);

		if (filling)
		{
			fillTime.Advance(dt);
			const float t = std::clamp(fillTime.GetElapsed() / fillDuration, 0.0f, 1.0f);
			SetValueImmediate(lifeRatio * t * t * (3.0f - 2.0f * t));
			filling = t < 1.0f;
		}
		else SetValue(lifeRatio);
	}
	GaugeHUD::OnUpdate();
	if (visibilityAnimating)
	{
		visibilityTime.Advance(dt);
		const float duration = showing ? showDuration : hideDuration;
		const float t = std::clamp(visibilityTime.GetElapsed() / duration, 0.0f, 1.0f);
		const float target = showing ? 0.0f : 1.0f;
		SetDissolveAmount(visibilityStart + (target - visibilityStart) * t * t * (3.0f - 2.0f * t));
		visibilityAnimating = t < 1.0f;
		if (!showing && !visibilityAnimating) SetActive(false);
	}
}

void BossBar::OnDrawGUI()
{
	GaugeHUD::OnDrawGUI();
	ImGui::SliderFloat((const char*)u8"出現時間", &showDuration, 0.05f, 2.0f);
	ImGui::SliderFloat((const char*)u8"消去時間", &hideDuration, 0.05f, 2.0f);
	ImGui::SliderFloat((const char*)u8"HP充填時間", &fillDuration, 0.05f, 3.0f);
}
