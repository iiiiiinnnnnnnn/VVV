// BoneCollider.h

#pragma once
#include <string>

#include "Physics/Core/CollidersDef.h"

class VMDLModel;

static Matrix MakeBoneOffsetWorld(const Matrix& boneWorld, const Matrix& offset)
{
    return offset * boneWorld;
}

class BoneCollider : public PhysicsComponent
{
public:
    BoneCollider(Object* owner, LayerId layerId, VMDLModel* model, int nodeIndex, Matrix offset, PxMaterial* material,
        bool isTrigger, bool freezePositions, bool freezeRotations);
    ~BoneCollider() override;
    void OnEnabled() override;
    void OnDisabled() override;
    void LateUpdate() override;
    Vector3 GetWorldPosition() const;
    Actor* FindOverlapActorByTag(const std::string& tag) const;

protected:
    void InitializeShape();
    void UpdateShape();
    virtual PxShape* CreateShape(PxPhysics* physics, PxMaterial* material) const = 0;
    virtual PxTransform GetLocalPose() const { return PxTransform(PxIdentity); }
    void DrawBoneSettingsGUI();

    PxShape* shape = nullptr;
    PxRigidDynamic* ghostActor = nullptr;
    PxMaterial* material = nullptr;
    VMDLModel* model = nullptr;
    int nodeIndex = -1;
    Matrix offset;
    bool isTrigger = true;
    bool freezePositions = false;
    bool freezeRotations = false;
};
