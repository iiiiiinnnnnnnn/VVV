// SpriteRenderComponent.cpp

#include "SpriteRenderComponent.h"
#include <Graphics.h>
#include "Widget.h"
#include "IconsFontAwesome5.h"

SpriteRenderComponent::SpriteRenderComponent(Object* owner, std::shared_ptr<Texture> texture, SpriteShaderId shaderId, ShaderParamList shaderParam)
	: Component(owner), texture(texture), shaderId(shaderId), shaderParam(shaderParam)
{
	// エラー用
	Component::GetOwnerAsWidget();
}

void SpriteRenderComponent::Update()
{
}

void SpriteRenderComponent::Render(const RenderContext& rc)
{
	Widget* widget = Component::GetOwnerAsWidget();
	if (texture)
	{
		// anchor分だけpositionをオフセット
		float x = widget->rect.position.x - widget->rect.size.x * widget->rect.anchor.x;
		float y = widget->rect.position.y - widget->rect.size.y * widget->rect.anchor.y;

		Game::Graphics::Instance().GetSpriteRenderer()->Draw(
			shaderId, texture,
			{ x, y, 0.0f },
			widget->rect.size,
			{ 0.0f, 0.0f },
			{ (float)texture->GetWidth(), (float)texture->GetHeight() },
			widget->rect.angle,
			shaderParam);
	}
}

void SpriteRenderComponent::DrawGUI()
{
	if (ImGui::TreeNode(ICON_FA_IMAGE " SpriteRenderComponent"))
	{
		if (texture)
		{
			ImGui::Text("Texture: %s", texture->GetShaderResourceView() ? "Loaded" : "Not Loaded");
		}
		else
		{
			ImGui::Text("Texture: None");
		}

		// シェーダーパラメータ
		if (ImGui::TreeNode(ICON_FA_PASTE " ShaderParams"))
		{
			for (ShaderParam& p : shaderParam)
			{
				std::visit(ParamGUIVisitor{p.name.c_str()}, p.value);
			}

			ImGui::TreePop();
		}

		ImGui::TreePop();
	}
}

ShaderParam* SpriteRenderComponent::FindShaderParam(const std::string& name)
{
	for (ShaderParam& param : shaderParam)
	{
		if (param.name == name)
		{
			return &param;
		}
	}

	return nullptr;
}

const ShaderParam* SpriteRenderComponent::FindShaderParam(const std::string& name) const
{
	for (const ShaderParam& param : shaderParam)
	{
		if (param.name == name)
		{
			return &param;
		}
	}

	return nullptr;
}

void SpriteRenderComponent::SetShaderParam(
	const std::string& name,
	const ParamValue& value)
{
	if (ShaderParam* param = FindShaderParam(name))
	{
		param->value = value;
		return;
	}

	shaderParam.push_back({ name, value });
}
