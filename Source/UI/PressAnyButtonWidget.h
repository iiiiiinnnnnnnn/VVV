#pragma once

#include "Application/Time/Timer.h"

#include "SpriteWidget.h"

class PressAnyButtonWidget : public SpriteWidget
{
  public:
	PressAnyButtonWidget(const std::string& spritePath);
	void Update() override;

  private:
	Game::Timer blinkTimer{4.0f};
};