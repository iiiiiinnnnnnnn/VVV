#include "UI/ColorWidget.h"
#include "Rendering/Core/Graphics.h"

ColorWidget::ColorWidget(SpriteShaderId shaderId, const Color& color)
	: SpriteWidget(std::filesystem::path{}, shaderId, color)
{
	SetName("Color Widget");
}

void ColorWidget::OnAwake()
{
	if (!GetTexture())
		SetTexture(std::make_shared<Texture>(Game::Graphics::Instance().GetDevice(), Color(1, 1, 1, 1)));
	SpriteWidget::OnAwake();
}
