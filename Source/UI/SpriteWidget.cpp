// SpriteWidget.cpp
#include "UI/SpriteWidget.h"
#include "Resource/ResourceManager.h"

#include "Rendering/Component/SpriteRenderComponent.h"

SpriteWidget::SpriteWidget(std::filesystem::path spritePath,
                           SpriteShaderId shaderId, const Color& color)
    : Widget(spritePath.filename().string().c_str())
{
    AddComponent<SpriteRenderComponent>(
        ResourceManager::Instance().LoadTexture(spritePath.generic_string()), shaderId, color);
}
