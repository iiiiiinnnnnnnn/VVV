// ModelRenderComponent.h

#pragma once

#include "Model.h"
#include "ModelRenderer.h"
#include "Component.h"

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

	const ModelShaderId& GetShaderId() const { return shaderId; }
    void SetShaderId(ModelShaderId id) { shaderId = id; }

    void AppendNode(const Model::Node* node) { appendNode = node; }
	const Model::Node* GetAppendNode() const { return appendNode; }

private:
    std::shared_ptr<Model> model;
	ModelShaderId shaderId;
    ShaderParamListWithMaterialName paramsWithMaterial;
    const Model::Node* appendNode = nullptr;
};
