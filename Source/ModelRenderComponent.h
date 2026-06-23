// ModelRenderComponent.h

#pragma once

#include "Component.h"
#include "Model.h"
#include "ModelRenderer.h"

class ModelRenderComponent : public Component {
public:
    ModelRenderComponent(Object* owner, std::shared_ptr<Model> model,
                         ModelShaderId shaderId = ModelShaderId::Basic,
                         ShaderParamListWithMaterialName paramsWithMaterial = {});

    void LateUpdate() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    Model* GetModel() const { return model.get(); }
    void SetModel(std::shared_ptr<Model> model) { this->model = model; }
    void SetShaderParamForAllMaterials(const ShaderParam& param);
    void SetShaderParamForAllMaterials(const ShaderParamList& paramList);

	const ModelShaderId& GetShaderId() const { return shaderId; }
    void SetShaderId(ModelShaderId id) { shaderId = id; }

	ShaderParamListWithMaterialName& GetParamsWithMaterial() { return paramsWithMaterial; }
	const ShaderParamListWithMaterialName& GetParamsWithMaterial() const { return paramsWithMaterial; }

private:
    std::shared_ptr<Model> model;
	ModelShaderId shaderId;
    ShaderParamListWithMaterialName paramsWithMaterial;
};
