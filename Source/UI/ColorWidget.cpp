// ColorWidget.cpp
#include "UI/ColorWidget.h"
#include "Rendering/Component/SpriteRenderComponent.h"

ColorWidget::ColorWidget(SpriteShaderId shaderId, const Color& color)
	: Widget("Color Widget")
{
	AddComponent<SpriteRenderComponent>(
		std::make_shared<Texture>(Color(1, 1, 1, 1)), shaderId, color);
}

void ColorWidget::SetColor(const Color& color)
{
	GetComponent<SpriteRenderComponent>()->SetColor(color);
}

const Color& ColorWidget::GetColor() const
{
	return GetComponent<SpriteRenderComponent>()->GetColor();
}
