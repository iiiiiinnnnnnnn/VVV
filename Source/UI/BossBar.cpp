#include "UI/BossBar.h"

#include <algorithm>
#include <cmath>

#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Entity.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include "Resource/ResourceManager.h"
#include "Resource/Texture.h"

BossBar::BossBar() : Widget("Boss Bar")
{
	frameTexture = ResourceManager::Instance().LoadTexture("Resources/UI/window.png");
	fillTexture = std::make_shared<Texture>(Color(1.0f, 1.0f, 1.0f, 1.0f));
	SetAffectedByPostProcess(false);
	SetActive(false);
}

void BossBar::Show(Entity* value)
{
	if (!value || value->IsPendingDestroy() || value->IsDead()) return;

	const bool changedBoss = boss != value;
	boss = value;
	lifeRatio = boss->GetMaxLife() > 0.0f
		? std::clamp(boss->GetLife() / boss->GetMaxLife(), 0.0f, 1.0f)
		: 0.0f;
	if (changedBoss) delayedLifeRatio = lifeRatio;
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

	lifeRatio = boss->GetMaxLife() > 0.0f
		? std::clamp(boss->GetLife() / boss->GetMaxLife(), 0.0f, 1.0f)
		: 0.0f;
	const float dt = std::max(Game::Time::unscaledDeltaTime, 0.0f);
	const float blend = 1.0f - std::exp(-4.5f * dt);
	delayedLifeRatio += (lifeRatio - delayedLifeRatio) * blend;

	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	const float hudScale = std::clamp(
		std::min(screenWidth / 1920.0f, screenHeight / 1080.0f), 0.68f, 1.35f);
	rect.position = {screenWidth * 0.5f, 22.0f * hudScale};
	rect.anchor = {0.5f, 0.0f};
	rect.size = {540.0f * hudScale, 58.0f * hudScale};
}

void BossBar::OnRender(const RenderContext&)
{
	if (!frameTexture || !fillTexture) return;

	SpriteRenderer* renderer = Game::Graphics::Instance().GetSpriteRenderer();
	const float scale = rect.size.x / 540.0f;
	const float x = rect.position.x - rect.size.x * rect.anchor.x;
	const float y = rect.position.y - rect.size.y * rect.anchor.y;

	renderer->Draw(
		SpriteShaderId::Basic, frameTexture,
		{x, y, 0.0f}, rect.size,
		{0.0f, 0.0f},
		{static_cast<float>(frameTexture->GetWidth()), static_cast<float>(frameTexture->GetHeight())},
		rect.angle, Color(0.32f, 0.27f, 0.34f, 0.94f));

	const float innerX = x + 54.0f * scale;
	const float innerY = y + 19.0f * scale;
	const float innerWidth = 432.0f * scale;
	const float innerHeight = 19.0f * scale;
	const float delayedWidth = innerWidth * std::clamp(delayedLifeRatio, 0.0f, 1.0f);
	const float currentWidth = innerWidth * lifeRatio;

	if (delayedWidth > 0.0f)
		renderer->Draw(SpriteShaderId::Basic, fillTexture,
			{innerX, innerY, 0.0f}, {delayedWidth, innerHeight},
			{0.0f, 0.0f}, {1.0f, 1.0f}, 0.0f,
			Color(0.95f, 0.58f, 0.08f, 0.95f));
	if (currentWidth > 0.0f)
		renderer->Draw(SpriteShaderId::Basic, fillTexture,
			{innerX, innerY, 0.0f}, {currentWidth, innerHeight},
			{0.0f, 0.0f}, {1.0f, 1.0f}, 0.0f,
			lifeRatio < 0.25f
				? Color(1.0f, 0.03f, 0.02f, 0.98f)
				: Color(0.68f, 0.025f, 0.055f, 0.98f));
}
