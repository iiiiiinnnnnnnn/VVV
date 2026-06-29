// BoneCollider.h

#pragma once

#include "CollidersDef.h"

class Model;

static Matrix MakeBoneOffsetWorld(const Matrix& boneWorld, const Matrix& offset)
{
    Vector3 scale;
    Vector3 position;
    Quaternion rotation;
    Matrix boneWorldCopy = boneWorld;
    boneWorldCopy.Decompose(scale, rotation, position);

    Matrix boneTransform =
        Matrix::CreateScale(scale) *
        Matrix::CreateFromQuaternion(rotation) *
        Matrix::CreateTranslation(position);

    return offset * boneTransform;
}

class BoneCollider : public PhysicsComponent
{
public:
    BoneCollider(Object* owner, LayerId layerId, Model* model, int nodeIndex, Matrix offset, PxMaterial* material,
        bool isTrigger, bool freezePositions, bool freezeRotations);
    ~BoneCollider() override;
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
    Model* model = nullptr;
    int nodeIndex = -1;
    Matrix offset;
    bool isTrigger = true;
    bool freezePositions = false;
    bool freezeRotations = false;
};