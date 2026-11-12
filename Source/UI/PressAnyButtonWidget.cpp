#include "pch.h"
#include "PressAnyButtonWidget.h"

#include "Application/Time/GameTime.h"

#include "Core/Foundation/Easing.h"

PressAnyButtonWidget::PressAnyButtonWidget()
	: SpriteWidget("PressAnyButton", "Resources/UI/pressAnyButton.png"){}

void PressAnyButtonWidget::Update()
{
	if (!IsActive()) return;
	SpriteWidget::Update();

	// アルファ点滅(Easing)

	blinkTimer.Tick(Game::Time::unscaledDeltaTime);
	const float elapsedTime = blinkTimer.GetDuration() - blinkTimer.GetRemaining();
	const float t = 1.0f - std::abs(elapsedTime * 0.5f - 1.0f);

	auto color = GetColor();
	color.w = Easing::Evaluate(t, Easing::Type::OutQuad);
	SetColor(color);
}
