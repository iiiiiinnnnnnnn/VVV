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
						  const Color& color = Color(1, 1, 1, 1));
    
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
	void SetVignetteParameters(float range, float softness)
	{
		shaderParameters.x = std::clamp(range, 0.001f, 1.0f);
		shaderParameters.y = std::clamp(softness, 0.001f, shaderParameters.x);
	}
	float GetVignetteRange() const { return shaderParameters.x; }
	float GetVignetteSoftness() const { return shaderParameters.y; }

private:
    std::shared_ptr<Texture> texture;
	SpriteShaderId shaderId;
	Color color;
	float horizontalFill = 1.0f;
	Vector4 shaderParameters = Vector4(0.92f, 0.92f, 0.0f, 0.0f);
};
