#pragma once

#include "UI/SpriteWidget.h"

class TitleLogoWidget final : public SpriteWidget
{
public:
	void OnAwake() override;
	TitleLogoWidget();

protected:
	void OnUpdate() override;

private:
	void UpdateLayout();
	int animation = 0;
	float progress = 0.0f;
	Vector2 screenSize = Vector2::Zero;
};