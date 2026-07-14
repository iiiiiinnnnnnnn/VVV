// SphereCollider.h

#pragma once

#include "CollidersDef.h"

class SphereCollider : public PhysicsComponent {
public:
    SphereCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, float radius, PxMaterial* material = nullptr);
    SphereCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, float radius, const Vector3& localPosition, PxMaterial* material = nullptr);
    void OnAwake() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
    const char* GetDebugName() const override { return ICON_FA_SHAPES " SphereCollider"; }

    const float GetRadius() const { return radius; }
    const Vector3& GetLocalPosition() const { return localPosition; }
private:
    PxTransform MakeLocalPose() const;
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    float radius;
    Vector3 localPosition;
};