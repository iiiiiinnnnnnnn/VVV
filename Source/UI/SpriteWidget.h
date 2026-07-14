// SpriteWidget.h

#pragma once

#include "UI/Widget.h"

#include "Rendering/Renderer/SpriteRenderer.h"
#include <filesystem>

class SpriteWidget : public Widget
{
public:
    SpriteWidget(std::filesystem::path spritePath,
                 SpriteShaderId shaderId = SpriteShaderId::Basic, ShaderParamList shaderParam = {});
};
