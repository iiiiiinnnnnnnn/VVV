#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Core/Object/Component.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Renderer/ModelRenderer.h"

class Animator;
class MeshCache;
class PhysicsComponent;
class VMDLColliderComponent;
class TrailRenderComponent;
class VMDLParticleEmitterComponent;

class VMDLModelComponent : public Component
{
  public:
	VMDLModelComponent(Object* owner, std::shared_ptr<VMDLModel> model,
		ModelShaderId shaderId = ModelShaderId::VMat, VMatRenderParams renderParams = {});

	void OnAwake() override;
	void LateUpdate() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_CUBES " VMDLModelComponent"; }

	VMDLModel* GetModel() const { return model.get(); }
	void SetModel(std::shared_ptr<VMDLModel> model) { this->model = model; }
	void SetAutoUpdateTransform(bool value) { autoUpdateTransform = value; }
	void SetModelYawOffset(float radians) { modelYawOffset = radians; }
	void UpdateModelTransform(const Matrix& actorTransform);
	bool PlaySoundSource(const std::string& name, const Vector3* positionOverride = nullptr);
	float BurstParticleEmitter(const std::string& name);
	void SetAttachmentLayerId(LayerId value) { attachmentLayerId = value; }

	const ModelShaderId& GetShaderId() const { return shaderId; }
	void SetShaderId(ModelShaderId id) { shaderId = id; }
	void SetMaterialParamsForAllMaterials(const VMatMaterialParams& params);
	VMatRenderParams& GetRenderParams() { return renderParams; }
	const VMatRenderParams& GetRenderParams() const { return renderParams; }
	void BuildAttachments();
	void SetBuildEmbeddedTrails(bool value) { buildEmbeddedTrails = value; }
	PhysicsComponent* GetAttachmentCollider(const std::string& name) const;
	void SetAttachmentLayerEnabled(LayerId layerId, bool enabled)
	{
		attachmentLayerEnabled[layerId] = enabled;
	}
	const std::vector<VMDLColliderComponent*>& GetAttachmentColliders() const
	{
		return attachmentColliders;
	}
	bool EquipMeshCache(
		const std::string& slot, const std::string& path, const std::string& fallbackNodeName = {});
	void UnequipMeshCache(const std::string& slot);
	const std::unordered_map<std::string, std::shared_ptr<MeshCache>>& GetMeshCaches() const
	{
		return meshCaches;
	}
	const std::unordered_map<int, std::shared_ptr<MeshCache>>& GetExternalMeshCaches() const
	{
		return externalMeshCaches;
	}

  private:
	void UpdateAnimationControls();
	void RestoreAnimationControls();
	void UpdateSoundEvents();
	void PlaySoundEvents(int animationIndex, float beginTime, float endTime);
	void SyncExternalMeshCaches();

	std::shared_ptr<VMDLModel> model;
	ModelShaderId shaderId;
	VMatRenderParams renderParams;
	bool autoUpdateTransform = true;
	LayerId attachmentLayerId = 0;
	bool buildEmbeddedTrails = true;
	bool attachmentsBuilt = false;
	bool animationControlsApplied = false;
	float modelYawOffset = 0.0f;
	Animator* animator = nullptr;
	int soundAnimationIndex = -1;
	float soundAnimationTime = 0.0f;
	struct ActiveSoundEvent
	{
		uint64_t voiceId = 0;
		int sourceIndex = -1;
	};
	std::vector<ActiveSoundEvent> activeSoundEvents;
	std::vector<VMDLColliderComponent*> attachmentColliders;
	std::vector<TrailRenderComponent*> attachmentTrails;
	std::vector<VMDLParticleEmitterComponent*> attachmentParticleEmitters;
	std::unordered_map<LayerId, bool> attachmentLayerEnabled;
	std::unordered_map<std::string, std::shared_ptr<MeshCache>> meshCaches;
	std::unordered_map<int, std::shared_ptr<MeshCache>> externalMeshCaches;
};
