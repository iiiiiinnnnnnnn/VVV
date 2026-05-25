// SpriteRenderComponent.h

#pragma once

#include "Texture.h"
#include "SpriteRenderer.h"
#include "Component.h"

class SpriteRenderComponent : public Component {
public:
    SpriteRenderComponent(Object* owner, std::shared_ptr<Texture> texture, SpriteShaderID shaderId = SpriteShaderID::Basic, ShaderParamPtr shaderParam = nullptr);
    
    void Update(float elapsedTime) override;
    void Render(const RenderContext& rc, float elapsedTime) override;
    void DrawGUI(float elapsedTime) override;

    void SetTexture(std::shared_ptr<Texture> texture) { this->texture = texture; }
    void SetShaderId(SpriteShaderID id) { shaderId = id; }

    Texture* GetTexture() const { return texture.get(); }
    const SpriteShaderID& GetShaderId() const { return shaderId; }

	const ShaderParamPtr& GetShaderParam() const { return shaderParam; }
	void SetShaderParam(ShaderParamPtr param) { shaderParam = param; }

private:
    std::shared_ptr<Texture> texture;
    SpriteShaderID shaderId;
    ShaderParamPtr shaderParam;
};
