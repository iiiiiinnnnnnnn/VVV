#include "UI/SpriteWidget.h"
#include "Resource/ResourceManager.h"

#include "Rendering/Component/SpriteRenderComponent.h"

SpriteWidget::SpriteWidget(std::filesystem::path spritePath,
						   SpriteShaderId shaderId, const Color& color,
						   SpriteRenderParams renderParams)
    : Widget(spritePath.filename().string()), spritePath(std::move(spritePath)),
	  shaderId(shaderId), color(color), renderParams(std::move(renderParams))
{
}

void SpriteWidget::OnAwake()
{
    AddComponent<SpriteRenderComponent>(
		ResourceManager::Instance().LoadTexture(spritePath.generic_string(), false), shaderId, color,
		std::move(renderParams));
}
