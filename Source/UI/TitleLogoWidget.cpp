#include "UI/TitleLogoWidget.h"

TitleLogoWidget::TitleLogoWidget(const std::string& name, const std::filesystem::path& spritePath)
	: SpriteWidget(name, spritePath)
{
	rect.position = {1479.0f, 576.8f};
	rect.angle = 0.0f;
	rect.size = {1526.0f, 625.0f};
	rect.scale = {0.36f, 0.43f};
	rect.anchor = {0.5f, 0.5f};
	SetAffectedByPostProcess(false);
}
