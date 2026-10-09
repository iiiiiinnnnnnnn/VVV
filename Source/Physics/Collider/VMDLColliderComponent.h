// VMDLColliderComponent.h
#pragma once
#include <string>

#include "Physics/Core/CollidersDef.h"

class Actor;
class VMDLModel;

class VMDLColliderComponent : public PhysicsComponent
{
  public:
	VMDLColliderComponent(Object* owner, LayerId layerId, VMDLModel* model, int nodeIndex,
		int shapeType, const Vector3& size, const Matrix& offset = Matrix::Identity,
		PxMaterial* material = nullptr, bool isTrigger = true);

	~VMDLColliderComponent() override;

	void OnEnabled() override;
	void OnDisabled() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;

	const char* GetDebugName() const override { return ICON_FA_SHAPES " VMDLColliderComponent"; }

	void UpdateFromNode();
	bool StartDynamicMotion(const Vector3& velocity, const Vector3& angularVelocity);
	void SyncOwnerTransform();
	Vector3 GetWorldPosition() const;
	bool GetBoundingSphere(Vector3& center, float& radius) const;
	Actor* FindOverlapActorByTag(const std::string& tag) const;

  private:
	void CreateShape();
	void CreateMeshShape();
	void UpdateScaledSize(const Matrix& world);
	PxTransform GetShapeLocalPose() const;

	VMDLModel* model = nullptr;
	int nodeIndex = -1;
	int shapeType = 0;
	Vector3 size = Vector3::One;
	Vector3 scaledSize = Vector3::One;
	Matrix offset = Matrix::Identity;
	bool isTrigger = true;
	bool sweepReady = false;
	bool drivesOwner = false;
	Matrix ownerFromBody = Matrix::Identity;

	PxMaterial* material = nullptr;
	PxRigidDynamic* ghostActor = nullptr;
	PxShape* shape = nullptr;
};
