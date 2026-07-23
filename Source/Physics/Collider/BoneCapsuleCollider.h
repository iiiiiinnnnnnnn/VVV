// BoneCapsuleCollider.h

#pragma once

#include "Physics/Core/CollidersDef.h"
#include "Physics/Collider/BoneCollider.h"

class VMDLModel;

class BoneCapsuleCollider : public BoneCollider
{
public:
    BoneCapsuleCollider(Object* owner, LayerId layerId, VMDLModel* model, int nodeIndex, float radius,
        float height, Matrix offset = Matrix::Identity, PxMaterial* material = nullptr,
        bool isTrigger = true, bool freezePositions = false, bool freezeRotations = false);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_SHAPES " BoneCapsuleCollider"; }

private:
    PxShape* CreateShape(PxPhysics* physics, PxMaterial* material) const override;
    PxTransform GetLocalPose() const override;
private:
    float radius = 0.5f;
    float height = 1.0f;
};
