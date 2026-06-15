// SpriteWidget.h

#pragma once

#include "Widget.h"

class SpriteWidget : public Widget
{
public:
    SpriteWidget(std::filesystem::path spritePath,
                 SpriteShaderId shaderId = SpriteShaderId::Basic, ShaderParamList shaderParam = {});
};