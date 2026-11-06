#pragma once

#include "UI/SpriteWidget.h"

class ColorWidget : public SpriteWidget
{
public:
	ColorWidget(SpriteShaderId shaderId = SpriteShaderId::Basic,
		const Color& color = Color(1, 1, 1, 1));
	void OnAwake() override;
};
