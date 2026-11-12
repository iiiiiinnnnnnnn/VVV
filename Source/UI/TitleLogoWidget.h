#pragma once

#include "UI/SpriteWidget.h"

class TitleLogoWidget final : public SpriteWidget
{
public:
	TitleLogoWidget(const std::string& name, const std::filesystem::path& spritePath);

};
