// BoneCapsuleCollider.h

#pragma once

#include "CollidersDef.h"
#include "BoneCollider.h"

class Model;

// ボーンを指定して追従　行列オフセットも設定できる。
class BoneCapsuleCollider : public BoneCollider
{
public:
    BoneCapsuleCollider(Object* owner, LayerId layerId, Model* model, int nodeIndex, float radius,
        float height, Matrix offset = Matrix::Identity, PxMaterial* material = nullptr,
        bool isTrigger = true, bool freezePositions = false, bool freezeRotations = false);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

private:
    PxShape* CreateShape(PxPhysics* physics, PxMaterial* material) const override;
    PxTransform GetLocalPose() const override;
private:
    float radius = 0.5f;
    float height = 1.0f;
};