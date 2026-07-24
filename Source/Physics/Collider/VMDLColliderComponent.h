#pragma once

#include <string>

#include "Physics/Core/CollidersDef.h"

class Actor;
class VMDLModel;

class VMDLColliderComponent : public PhysicsComponent
{
public:
    VMDLColliderComponent(
        Object* owner,
        LayerId layerId,
        VMDLModel* model,
        int nodeIndex,
        int shapeType,
        const Vector3& size,
        const Matrix& offset = Matrix::Identity,
        PxMaterial* material = nullptr,
        bool isTrigger = true);

    ~VMDLColliderComponent() override;

    void OnEnabled() override;
    void OnDisabled() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    const char* GetDebugName() const override
    {
        return ICON_FA_SHAPES " VMDLColliderComponent";
    }

    void UpdateFromNode();
    Vector3 GetWorldPosition() const;
    Actor* FindOverlapActorByTag(const std::string& tag) const;

private:
    void CreateShape();
    PxTransform GetShapeLocalPose() const;

    VMDLModel* model = nullptr;
    int nodeIndex = -1;
    int shapeType = 0;
    Vector3 size = Vector3::One;
    Matrix offset = Matrix::Identity;
    bool isTrigger = true;

    PxMaterial* material = nullptr;
    PxRigidDynamic* ghostActor = nullptr;
    PxShape* shape = nullptr;
};
