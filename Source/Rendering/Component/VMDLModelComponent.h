// VMDLModelComponent.h

#pragma once
#include <memory>

#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Core/Object/Component.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Renderer/ModelRenderer.h"

class Animator;
class PhysicsComponent;
class TrailRenderComponent;

class VMDLModelComponent : public Component {
public:
    VMDLModelComponent(Object* owner, std::shared_ptr<VMDLModel> model,
                         ModelShaderId shaderId = ModelShaderId::Basic,
                         ShaderParamListWithMaterialName paramsWithMaterial = {});

	void OnAwake() override;
    void LateUpdate() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_CUBES " VMDLModelComponent"; }

    VMDLModel* GetModel() const { return model.get(); }
    void SetModel(std::shared_ptr<VMDLModel> model) { this->model = model; }
    void SetAutoUpdateTransform(bool value) { autoUpdateTransform = value; }
	void SetAttachmentLayerId(LayerId value) { attachmentLayerId = value; }
    void SetShaderParamForAllMaterials(const ShaderParam& param);
    void SetShaderParamForAllMaterials(const ShaderParamList& paramList);

	const ModelShaderId& GetShaderId() const { return shaderId; }
    void SetShaderId(ModelShaderId id) { shaderId = id; }
	void BuildAttachments();
	void SetBuildEmbeddedTrails(bool value) { buildEmbeddedTrails = value; }
	PhysicsComponent* GetAttachmentCollider(const std::string& name) const;

	ShaderParamListWithMaterialName& GetParamsWithMaterial() { return paramsWithMaterial; }
	const ShaderParamListWithMaterialName& GetParamsWithMaterial() const { return paramsWithMaterial; }

private:
	void UpdateAnimationControls();
	void RestoreAnimationControls();

    std::shared_ptr<VMDLModel> model;
	ModelShaderId shaderId;
    ShaderParamListWithMaterialName paramsWithMaterial;
	bool autoUpdateTransform = true;
	LayerId attachmentLayerId = 0;
	bool buildEmbeddedTrails = true;
	bool attachmentsBuilt = false;
	bool animationControlsApplied = false;
	Animator* animator = nullptr;
	std::vector<PhysicsComponent*> attachmentColliders;
	std::vector<TrailRenderComponent*> attachmentTrails;
	std::vector<uint8_t> initialMeshVisibility;
};
