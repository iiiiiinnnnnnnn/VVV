// SpriteWidget.h

#pragma once

#include "Widget.h"

#include "SpriteRenderer.h"
#include <filesystem>

class SpriteWidget : public Widget
{
public:
    SpriteWidget(std::filesystem::path spritePath,
                 SpriteShaderId shaderId = SpriteShaderId::Basic, ShaderParamList shaderParam = {});
};
