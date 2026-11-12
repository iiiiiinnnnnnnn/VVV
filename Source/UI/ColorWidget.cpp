#include "UI/ColorWidget.h"
#include "Rendering/Core/Graphics.h"

ColorWidget::ColorWidget(const std::string& name, SpriteShaderId shaderId, const Color& color)
	: SpriteWidget(name, std::filesystem::path{}, shaderId, color)
{
}

void ColorWidget::OnAwake()
{
	if (!GetTexture())
		SetTexture(std::make_shared<Texture>(Game::Graphics::Instance().GetDevice(), Color(1, 1, 1, 1)));
	SpriteWidget::OnAwake();
}
