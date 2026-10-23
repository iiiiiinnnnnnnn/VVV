#include "UI/TitleLogo.h"

#include <algorithm>
#include "Application/Time/GameTime.h"
#include "UI/UILayout.h"

TitleLogo::TitleLogo() : SpriteWidget("Resources/UI/titleLogo.png")
{
	SetName("TitleLogo");
	UpdateLayout();
}

void TitleLogo::OnUpdate()
{
	UpdateLayout();
}

void TitleLogo::UpdateLayout()
{
	UILayout::Image(*this, {0.50f, 0}, {0.50f, 0.45f}, {0, 268.8f}, {0.5f, 0.5f});
}
