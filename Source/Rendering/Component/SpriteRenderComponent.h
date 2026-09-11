#pragma once
#include <memory>
#include <string>
#include <algorithm>

#include "Resource/Texture.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include "Core/Object/Component.h"

class SpriteRenderComponent : public Component {
public:
    SpriteRenderComponent(Object* owner, std::shared_ptr<Texture> texture,
                          SpriteShaderId shaderId = SpriteShaderId::Basic,
						  const Color& color = Color(1, 1, 1, 1),
						  SpriteRenderParams renderParams = {});
    
    void Update() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_IMAGE " SpriteRenderComponent"; }

    Texture* GetTexture() const { return texture.get(); }
    void SetTexture(std::shared_ptr<Texture> texture) { this->texture = texture; }

    const SpriteShaderId& GetShaderId() const { return shaderId; }
    void SetShaderId(SpriteShaderId id) { shaderId = id; }
	const Color& GetColor() const { return color; }
	void SetColor(const Color& value) { color = value; }
	void SetHorizontalFill(float value) { horizontalFill = std::clamp(value, 0.0f, 1.0f); }
	float GetHorizontalFill() const { return horizontalFill; }
	void SetSourceRect(const Vector2& position, const Vector2& size)
	{
		sourcePosition = position;
		sourceSize = size;
		useSourceRect = true;
	}
	void ClearSourceRect() { useSourceRect = false; }
	SpriteRenderParams& GetRenderParams() { return renderParams; }
	const SpriteRenderParams& GetRenderParams() const { return renderParams; }

private:
    std::shared_ptr<Texture> texture;
	SpriteShaderId shaderId;
	Color color;
	float horizontalFill = 1.0f;
	Vector2 sourcePosition = Vector2::Zero;
	Vector2 sourceSize = Vector2::Zero;
	bool useSourceRect = false;
	SpriteRenderParams renderParams;
};
