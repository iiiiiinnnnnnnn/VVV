// ModelRenderComponent.h

#pragma once

#include "Model.h"
#include "ModelRenderer.h"
#include "Component.h"

class ModelRenderComponent : public Component {
public:
    ModelRenderComponent(Object* owner, std::shared_ptr<Model> model, ModelShaderId shaderId = ModelShaderId::Basic);

    void LateUpdate(float elapsedTime) override;
    void Render(const RenderContext& rc, float elapsedTime) override;
    void DrawGUI(float elapsedTime) override;

    void SetModel(std::shared_ptr<Model> model) { this->model = model; }
    void SetShaderId(ModelShaderId id) { shaderId = id; }
    void AppendNode(const Model::Node* node) { appendNode = node; }

    Model* GetModel() const { return model.get(); }
	const ModelShaderId& GetShaderId() const { return shaderId; }
	const Model::Node* GetAppendNode() const { return appendNode; }

private:
    std::shared_ptr<Model> model;
	ModelShaderId shaderId;
    const Model::Node* appendNode = nullptr;
};
