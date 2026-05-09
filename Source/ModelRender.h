// ModelRender.h

#pragma once

#include "Model.h"
#include "ModelRenderer.h"
#include "Graphics.h"
#include "Component.h"
#include "Actor.h"
#include "Transform.h"

class ModelRender : public Component {
public:
    ModelRender(Actor* owner, std::shared_ptr<Model> model, ShaderId shaderId = ShaderId::Lambert);

    void Update(float elapsedTime) override;
    void Render(const RenderContext& rc, float elapsedTime) override;
    void DrawGUI(float elapsedTime) override;

    void SetModel(std::shared_ptr<Model> model) { this->model = model; }
    void SetShaderId(ShaderId id) { shaderId = id; }
    void AppendNode(const Model::Node* node) { appendNode = node; }

    Model* GetModel() const { return model.get(); }
	const ShaderId& GetShaderId() const { return shaderId; }
	const Model::Node* GetAppendNode() const { return appendNode; }

private:
    std::shared_ptr<Model> model;
	ShaderId shaderId;
    const Model::Node* appendNode = nullptr;
};
