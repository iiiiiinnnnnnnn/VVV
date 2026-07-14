// BoxCollider.h

#include "CollidersDef.h"

#pragma once

class BoxCollider : public PhysicsComponent {
public:
    BoxCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, const Vector3& size, PxMaterial* material = nullptr);
    BoxCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, const Vector3& size, const Vector3& localPosition, PxMaterial* material = nullptr);
    void OnAwake() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_SHAPES " BoxCollider"; }

    const Vector3& GetSize() const { return size; }
    const Vector3& GetLocalPosition() const { return localPosition; }
private:
    PxTransform MakeLocalPose() const;
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    Vector3 size = Vector3::One;
    Vector3 localPosition = Vector3::Zero;
};