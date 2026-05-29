// SpriteRenderComponent.h

#pragma once

#include "Texture.h"
#include "SpriteRenderer.h"
#include "Component.h"

class SpriteRenderComponent : public Component {
public:
    SpriteRenderComponent(Object* owner, std::shared_ptr<Texture> texture,
                          SpriteShaderId shaderId = SpriteShaderId::Basic, ShaderParamPtr shaderParam = nullptr);
    
    void Update(float elapsedTime) override;
    void Render(const RenderContext& rc, float elapsedTime) override;
    void DrawGUI(float elapsedTime) override;

    Texture* GetTexture() const { return texture.get(); }
    void SetTexture(std::shared_ptr<Texture> texture) { this->texture = texture; }

    const SpriteShaderId& GetShaderId() const { return shaderId; }
    void SetShaderId(SpriteShaderId id) { shaderId = id; }

	const ShaderParamPtr& GetShaderParam() const { return shaderParam; }
	void SetShaderParam(ShaderParamPtr param) { shaderParam = param; }

private:
    std::shared_ptr<Texture> texture;
    SpriteShaderId shaderId;
    ShaderParamPtr shaderParam;
};
