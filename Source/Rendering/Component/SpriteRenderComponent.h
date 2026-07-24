// SpriteRenderComponent.h

#pragma once
#include <memory>
#include <string>

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

private:
    std::shared_ptr<Texture> texture;
    SpriteShaderId shaderId;
	Color color;
};
