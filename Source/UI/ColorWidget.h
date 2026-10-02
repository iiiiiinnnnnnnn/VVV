// ColorWidget.h
#pragma once

#include "UI/Widget.h"
#include "Rendering/Renderer/SpriteRenderer.h"

class ColorWidget : public Widget
{
public:
	ColorWidget(SpriteShaderId shaderId = SpriteShaderId::Basic,
		const Color& color = Color(1, 1, 1, 1));
	void SetColor(const Color& color);
	const Color& GetColor() const;
};
