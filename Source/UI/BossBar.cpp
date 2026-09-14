#include "UI/BossBar.h"

#include <algorithm>
#include "Gameplay/Actor/Entity.h"
#include "Rendering/Core/Graphics.h"

BossBar::BossBar() : GaugeHUD("Boss Bar")
{
	SetFrameTexture("Resources/UI/window.png");
	SetFillRect({0.10f, 0.33f}, {0.80f, 0.33f});
	SetColors(Color(0.68f, 0.025f, 0.055f, 0.98f),
		Color(1.0f, 0.03f, 0.02f, 0.98f), Color(0.32f, 0.27f, 0.34f, 0.94f));
	SetSmoothingSpeed(5.0f);
	SetAffectedByPostProcess(false);
	SetActive(false);
}

void BossBar::Show(Entity* value)
{
	if (!value || value->IsPendingDestroy() || value->IsDead()) return;

	const bool changedBoss = boss != value;
	boss = value;
	const float ratio = boss->GetMaxLife() > 0.0f
		? std::clamp(boss->GetLife() / boss->GetMaxLife(), 0.0f, 1.0f)
		: 0.0f;
	SetTargetValue(ratio);
	if (changedBoss) SnapToTarget();
	SetActive(true);
}

void BossBar::Hide(const Entity* value)
{
	// 別のボスから遅れて届いたロスト通知で、現在のバーを消さない。
	if (value && value != boss) return;
	boss = nullptr;
	SetActive(false);
}

void BossBar::OnUpdate()
{
	if (!boss || boss->IsPendingDestroy() || !boss->IsActive() || boss->IsDead())
	{
		Hide(boss);
		return;
	}

	const float lifeRatio = boss->GetMaxLife() > 0.0f
		? std::clamp(boss->GetLife() / boss->GetMaxLife(), 0.0f, 1.0f)
		: 0.0f;
	SetTargetValue(lifeRatio);
	GaugeHUD::OnUpdate();

	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	const float hudScale = std::clamp(
		std::min(screenWidth / 1920.0f, screenHeight / 1080.0f), 0.68f, 1.35f) *
		0.78f;
	rect.position = {screenWidth * 0.5f, 22.0f * hudScale};
	rect.anchor = {0.5f, 0.0f};
	rect.size = {540.0f * hudScale, 58.0f * hudScale};
}
