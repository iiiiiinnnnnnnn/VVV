#include "Rendering/Component/SpriteRendererComponent.h"
#include "UI/Widget.h"
#include "Resource/ResourceManager.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Core/RenderContext.h"
#include "Rendering/Renderer/SpriteRenderer.h"

SpriteRendererComponent::SpriteRendererComponent(Object* owner, const std::string& path)
	: Component(owner), path(path)
{
}

void SpriteRendererComponent::OnAwake()
{
	texture = ResourceManager::Instance().LoadTexture(path);
}

void SpriteRendererComponent::SetTexture(const std::string& value)
{
	path = value;
	if (isAwake || isAwaking) OnAwake();
}

void SpriteRendererComponent::OnRender(const RenderContext& rc)
{
	const RectTransform* rect = owner->GetRectTransform();
	if (!rect || !texture || fill <= 0.0f || color.w <= 0.0f) return;
	const Vector2 position = rect->size * localPosition;
	Vector2 size = rect->size * localSize;
	Vector2 source = sourceSize;
	if (source.x <= 0.0f || source.y <= 0.0f)
		source = {static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight())};
	// 描画幅とUV幅を同じ割合で切るため、残量が減っても模様を潰さない
	size.x *= fill;
	source.x *= fill;
	rc.graphics->GetSpriteRenderer()->Draw(renderParams.dissolve.mask ? SpriteShaderId::Dissolve : SpriteShaderId::Basic,
		texture, {position.x, position.y, 0.0f}, size, sourcePosition, source, 0.0f, color, &renderParams);
}
