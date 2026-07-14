// BoneBoxCollider.h

#pragma once

#include "Physics/Core/CollidersDef.h"
#include "Physics/Collider/BoneCollider.h"

class Model;

// ボーンを指定して追従　行列オフセットも設定できる。
class BoneBoxCollider : public BoneCollider
{
public:
    BoneBoxCollider(Object* owner, LayerId layerId, Model* model, int nodeIndex, const Vector3& size,
        Matrix offset = Matrix::Identity, PxMaterial* material = nullptr,
        bool isTrigger = true, bool freezePositions = false, bool freezeRotations = false);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_SHAPES " BoneBoxCollider"; }

private:
    PxShape* CreateShape(PxPhysics* physics, PxMaterial* material) const override;
private:
    Vector3 size = Vector3::One;
};