#pragma once

#include "Application/Time/Timer.h"

#include "SpriteWidget.h"

class PressAnyButton : public SpriteWidget
{
  public:
	PressAnyButton(const std::string& spritePath);
	void Update() override;

  private:
	Game::Timer blinkTimer{4.0f};
};