// ColorWidget.cpp
#include "UI/ColorWidget.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Component/SpriteRenderComponent.h"

ColorWidget::ColorWidget(SpriteShaderId shaderId, const Color& color)
	: Widget("Color Widget"), shaderId(shaderId), color(color)
{
}

void ColorWidget::OnAwake()
{
	AddComponent<SpriteRenderComponent>(
		std::make_shared<Texture>(Game::Graphics::Instance().GetDevice(), Color(1, 1, 1, 1)), shaderId, color);
}

void ColorWidget::SetColor(const Color& color)
{
	this->color = color;
	if (auto* sprite = GetComponent<SpriteRenderComponent>()) sprite->SetColor(color);
}

const Color& ColorWidget::GetColor() const
{
	if (auto* sprite = GetComponent<SpriteRenderComponent>()) return sprite->GetColor();
	return color;
}
