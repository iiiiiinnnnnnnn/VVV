// BoneSphereCollider.h

#pragma once

#include "CollidersDef.h"
#include "BoneCollider.h"

class Model;

// ボーンを指定して追従　行列オフセットも設定できる。
class BoneSphereCollider : public BoneCollider
{
public:
    BoneSphereCollider(Object* owner, LayerId layerId, Model* model, int nodeIndex, float radius,
        Matrix offset = Matrix::Identity, PxMaterial* material = nullptr,
        bool isTrigger = true, bool freezePositions = false, bool freezeRotations = false);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

private:
    PxShape* CreateShape(PxPhysics* physics, PxMaterial* material) const override;
private:
    float radius = 0.5f;
};