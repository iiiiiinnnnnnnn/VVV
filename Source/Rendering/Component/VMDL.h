#pragma once

#include <memory>
#include <string>

#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Core/Object/Component.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Component/VMDLModelComponent.h"

class Animator;
class HumanoidFootIK;
class MultiLegFootIK;
class PhysicsComponent;

class VMDL : public Component
{
  public:
	VMDL(Object* owner, const std::string& path);

	const char* GetDebugName() const override { return ICON_FA_CUBES " VMDL"; }
	void OnDrawGUI() override;

	const std::string& GetPath() const { return path; }
	const std::shared_ptr<VMDLModel>& GetSharedModel() const { return model; }
	VMDLModel* GetModel() const { return model.get(); }
	VMDLModelComponent* GetRenderer() const { return renderer; }
	Animator* GetAnimator() const { return animator; }
	HumanoidFootIK* GetHumanFootIK() const { return humanFootIK; }
	MultiLegFootIK* GetMultiLegFootIK() const { return multiLegFootIK; }
	PhysicsComponent* GetCollider(const std::string& name) const;
	VMDLModel::VmdlSoundSource* GetSoundSource(const std::string& name);
	const VMDLModel::VmdlSoundSource* GetSoundSource(const std::string& name) const;
	void SetAutoUpdateTransform(bool value);
	void SetModelYawOffset(float radians)
	{
		if (renderer) renderer->SetModelYawOffset(radians);
	}
	void UpdateTransform(const Matrix& actorTransform)
	{
		if (renderer) renderer->UpdateModelTransform(actorTransform);
	}
	bool ApplyMorph(const std::string& morphName);
	bool EquipMeshCache(
		const std::string& slot, const std::string& path, const std::string& fallbackNodeName = {});
	void UnequipMeshCache(const std::string& slot);

	template <typename T> T* GetCollider(const std::string& name) const
	{
		return dynamic_cast<T*>(GetCollider(name));
	}

  private:
	LayerId GetAttachmentLayer() const;
	void BuildFootIK();

	std::string path;
	std::shared_ptr<VMDLModel> model;
	VMDLModelComponent* renderer = nullptr;
	Animator* animator = nullptr;
	HumanoidFootIK* humanFootIK = nullptr;
	MultiLegFootIK* multiLegFootIK = nullptr;
};
