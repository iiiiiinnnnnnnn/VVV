// CapsuleCollider.h

#include "CollidersDef.h"

class CapsuleCollider : public PhysicsComponent {
public:
    CapsuleCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, float radius, float height, PxMaterial* material = nullptr);
    CapsuleCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, float radius, float height, const Vector3& localPosition, PxMaterial* material = nullptr);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_SHAPES " CapsuleCollider"; }

    const float GetRadius() const { return radius; }
    const float GetHeight() const { return height; }
    const Vector3& GetLocalPosition() const { return localPosition; }
private:
    PxTransform MakeLocalPose() const;
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    float radius;
    float height;
    Vector3 localPosition;
};