// BoneSphereCollider.h

#pragma once

#include "Physics/Core/CollidersDef.h"
#include "Physics/Collider/BoneCollider.h"

class VMDLModel;

class BoneSphereCollider : public BoneCollider
{
public:
    BoneSphereCollider(Object* owner, LayerId layerId, VMDLModel* model, int nodeIndex, float radius,
        Matrix offset = Matrix::Identity, PxMaterial* material = nullptr,
        bool isTrigger = true, bool freezePositions = false, bool freezeRotations = false);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_SHAPES " BoneSphereCollider"; }

private:
    PxShape* CreateShape(PxPhysics* physics, PxMaterial* material) const override;
private:
    float radius = 0.5f;
};
