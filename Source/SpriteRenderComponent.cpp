// SpriteRenderComponent.cpp

#include "SpriteRenderComponent.h"
#include <Graphics.h>
#include "Widget.h"

SpriteRenderComponent::SpriteRenderComponent(Object* owner, std::shared_ptr<Texture> texture, SpriteShaderId shaderId, ShaderParamPtr shaderParam)
	: Component(owner), texture(texture), shaderId(shaderId), shaderParam(shaderParam)
{
	Widget* widget = dynamic_cast<Widget*>(owner);
	_ASSERT_EXPR(widget != nullptr, L"Object is not Widget");
}

void SpriteRenderComponent::Update(float elapsedTime)
{
}

void SpriteRenderComponent::Render(const RenderContext& rc, float elapsedTime)
{
	Widget* widget = dynamic_cast<Widget*>(owner);
	_ASSERT_EXPR(widget != nullptr, L"Object is not Widget");

	if (texture)
	{
		Graphics::Instance().GetSpriteRenderer()->Draw(
			shaderId, texture,
			{ widget->rect.position.x, widget->rect.position.y, 0.0f },
			widget->rect.size,
			{ 0.0f, 0.0f },
			{ (float)texture->GetWidth(), (float)texture->GetHeight() },
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			shaderParam);
	}
}

void SpriteRenderComponent::DrawGUI(float elapsedTime)
{
    if (ImGui::TreeNode("SpriteRenderComponent"))
    {
        if (texture)
        {
			ImGui::Text("Texture: %s", texture->GetShaderResourceView() ? "Loaded" : "Not Loaded");
		}
		else
		{
			ImGui::Text("Texture: None");
        }
        ImGui::TreePop();
    }
}
