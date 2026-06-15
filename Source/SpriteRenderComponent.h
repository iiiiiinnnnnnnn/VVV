// SpriteRenderComponent.h

#pragma once

#include "Texture.h"
#include "SpriteRenderer.h"
#include "Component.h"

class SpriteRenderComponent : public Component {
public:
    SpriteRenderComponent(Object* owner, std::shared_ptr<Texture> texture,
                          SpriteShaderId shaderId = SpriteShaderId::Basic, ShaderParamList shaderParam = {});
    
    void Update() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    Texture* GetTexture() const { return texture.get(); }
    void SetTexture(std::shared_ptr<Texture> texture) { this->texture = texture; }

    const SpriteShaderId& GetShaderId() const { return shaderId; }
    void SetShaderId(SpriteShaderId id) { shaderId = id; }

    ShaderParamList& GetShaderParams() { return shaderParam; }
    const ShaderParamList& GetShaderParams() const { return shaderParam; }

    ShaderParam* FindShaderParam(const std::string& name);
    const ShaderParam* FindShaderParam(const std::string& name) const;
    void SetShaderParam(const std::string& name, const ParamValue& value);

private:
    std::shared_ptr<Texture> texture;
    SpriteShaderId shaderId;
    ShaderParamList shaderParam;
};
