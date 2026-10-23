#include "pch.h"
#include "PressAnyButton.h"

#include "Application/Time/GameTime.h"
#include "Rendering/Component/SpriteRenderComponent.h"

#include "Core/Foundation/Easing.h"

PressAnyButton::PressAnyButton(const std::string& spritePath) : SpriteWidget(spritePath) {}

void PressAnyButton::Update()
{
	if (!IsActive()) return;
	SpriteWidget::Update();

	// アルファ点滅(Easing)

	blinkTimer.Tick(Game::Time::unscaledDeltaTime);
	const float elapsedTime = blinkTimer.GetDuration() - blinkTimer.GetRemaining();
	const float t = 1.0f - std::abs(elapsedTime * 0.5f - 1.0f);

	if (auto* sprite = GetComponent<SpriteRenderComponent>())
	{
		auto color = sprite->GetColor();
		color.w = Easing::Evaluate(t, Easing::Type::OutQuad);
		sprite->SetColor(color);
	}
}