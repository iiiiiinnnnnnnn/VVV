#pragma once

#include "Core/Object/Component.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include <memory>
#include <string>
#include "IconsFontAwesome5.h"

class SpriteRenderer;
class Texture;

class SpriteButton final : public Component
{
  public:
	SpriteButton(Object* owner, const std::string& backPath, const std::string& frontPath, const Color& backColor = {1, 1, 1, 1}, const Color& frontColor = {1, 1, 1, 1});
	const char* GetDebugName() const override
	{
		return ICON_FA_MOUSE " スプライトボタン";
	}
	void OnAwake() override;
	void OnUpdate() override;
	void OnRender(const RenderContext& rc) override;
	void OnDisabled() override
	{
		frontAlpha = 0.0f;
	}
	void OnDrawGUI() override;

	void SetBackOffset(const Vector2& offset)
	{
		backOffset = offset;
	}
	void SetFrontOffset(const Vector2& offset)
	{
		frontOffset = offset;
	}
	void SetBackColor(const Color& color)
	{
		backColor = color;
	}
	void SetFrontColor(const Color& color)
	{
		frontColor = color;
	}
	void SetBlendSpeed(float speed)
	{
		blendSpeed = speed;
	}

  private:
	std::string backPath;
	std::string frontPath;
	std::shared_ptr<Texture> backTexture;
	std::shared_ptr<Texture> frontTexture;
	Vector2 backOffset = {0, 0};
	Vector2 frontOffset = {0, 0};
	Color backColor = {1, 1, 1};
	Color frontColor = {1, 1, 1};
	float frontAlpha = 0.0f;
	float blendSpeed = 14.0f;
};