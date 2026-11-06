#include "UI/TitleLogoWidget.h"

#include <algorithm>
#include "Application/Time/GameTime.h"
#include "UI/UILayout.h"

TitleLogoWidget::TitleLogoWidget() : SpriteWidget("Resources/UI/titleLogo.png")
{
	SetName("TitleLogoWidget");
}

void TitleLogoWidget::OnAwake()
{
	SpriteWidget::OnAwake();
	UpdateLayout();
}

void TitleLogoWidget::OnUpdate()
{
	UpdateLayout();
}

void TitleLogoWidget::UpdateLayout()
{
	UILayout::Image(*this, {0.50f, 0}, {0.50f, 0.45f}, {0, 268.8f}, {0.5f, 0.5f});
}
