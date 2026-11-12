#pragma once

#include "Core/Object/Component.h"
#include <ImGuiToolKit/Utf8.h>

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
		return JText(ICON_FA_MOUSE u8" スプライトボタン");
	}
	void OnAwake() override;
	void OnUpdate() override;
	void OnRender(const RenderContext& rc) override;
	void OnDisabled() override
	{
		frontAlpha = 0.0f;
	}
	void OnDrawGUI() override;

	void SetBackColor(const Color& color)
	{
		backColor = color;
	}
	void SetFrontColor(const Color& color)
	{
		frontColor = color;
	}

  private:
	std::string backPath;
	std::string frontPath;
	std::shared_ptr<Texture> backTexture;
	std::shared_ptr<Texture> frontTexture;
	Color backColor = {1, 1, 1};
	Color frontColor = {1, 1, 1};
	float frontAlpha = 0.0f;
};