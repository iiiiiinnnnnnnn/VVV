#include "UI/BossBarWidget.h"
#include "Rendering/Component/SpriteRendererComponent.h"

#include <algorithm>
#include "Gameplay/Actor/Entity.h"
#include "Rendering/Core/Graphics.h"

BossBarWidget::BossBarWidget(const std::string& name, const std::string& maskPath, bool revealOnStart)
	: GaugeWidget(name, maskPath, revealOnStart)
{
	rect.position = {Game::Graphics::ScreenWidth / 2.0f, 16.0f};
	rect.size = {700.0f, 35.0f};
	rect.anchor = {0.5f, 0.0f};
	SetDissolveAmount(1.0f);
	SetVisibility(false);
	SetValueImmediate(0.0f);
}

void BossBarWidget::OnAwake()
{
	DissolveWidget::OnAwake();
	fillPosition = {135.0f / 4072.0f, 52.0f / 221.0f};
	fillSize = {3802.0f / 4072.0f, 103.0f / 221.0f};
	backgroundSprite = AddComponent<SpriteRendererComponent>("Resources/UI/guage_back_boss.png");
	damageSprite = AddComponent<SpriteRendererComponent>("Resources/UI/guage_back_boss.png");
	fillSprite = AddComponent<SpriteRendererComponent>("Resources/UI/guage_back_boss.png");
	frameSprite = AddComponent<SpriteRendererComponent>("Resources/UI/guage_front_boss.png");
	for (auto* sprite : {backgroundSprite, damageSprite, fillSprite})
		sprite->SetSourceRect({135.0f, 52.0f}, {3802.0f, 103.0f});
}

void BossBarWidget::Show(Entity* value)
{
	if (!value || value->IsPendingDestroy() || value->IsDead()) return;

	const bool changedBoss = boss != value;
	boss = value;
	bossLifetime = value->weak_from_this();
	const float ratio = boss->GetMaxLife() > 0.0f
		? std::clamp(boss->GetLife() / boss->GetMaxLife(), 0.0f, 1.0f)
		: 0.0f;
	if (changedBoss || !showing) SetValueImmediate(0.0f);
	SetValue(ratio);
	showing = true;
	SetVisibility(true);
}

void BossBarWidget::Hide(const Entity* value)
{
	// 別のボスから遅れて届いたロスト通知で、現在のバーを消さない
	if (value && value != boss) return;
	if (!showing) return;
	const auto bossOwner = bossLifetime.lock();
	if (bossOwner && boss->IsDead()) SetValue(0.0f);
	boss = nullptr;
	bossLifetime.reset();
	showing = false;
	SetVisibility(false);
}

void BossBarWidget::OnUpdate()
{
	const auto bossOwner = bossLifetime.lock();
	if (showing && (!bossOwner || boss->IsPendingDestroy() || !boss->IsActive() || boss->IsDead()))
	{
		Hide(boss);
	}

	if (showing)
	{
		const float lifeRatio = boss->GetMaxLife() > 0.0f
			? std::clamp(boss->GetLife() / boss->GetMaxLife(), 0.0f, 1.0f) : 0.0f;
		SetValue(lifeRatio);
	}
	GaugeWidget::OnUpdate();
}
