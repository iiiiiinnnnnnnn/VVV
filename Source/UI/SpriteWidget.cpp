// SpriteWidget.cpp

#include "UI/SpriteWidget.h"

#include "Rendering/Component/SpriteRenderComponent.h"

SpriteWidget::SpriteWidget(std::filesystem::path spritePath,
                           SpriteShaderId shaderId, ShaderParamList shaderParam)
    : Widget(spritePath.filename().string().c_str())
{
    AddComponent<SpriteRenderComponent>(
        std::make_shared<Texture>(spritePath.string().c_str()), shaderId, shaderParam);
}