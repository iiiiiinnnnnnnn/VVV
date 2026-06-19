// CharacterController.h

#pragma once

#include "Component.h"
#include "PhysicsManager.h"
#include "Object.h"

class CharacterController : public Component {
public:
    CharacterController(Object* owner, float radius, float height);
    ~CharacterController();
    void Update() override;
    void LateUpdate() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
    void Move(const Vector3& velocity);
    void SetPosition(const Vector3& pos);
    void SetFootPosition(const Vector3& pos);
    void SetUseGravity(bool value) { useGravity = value; }
    bool IsGrounded() const { return grounded; }
private:
    void ApplyGravity();
    void SyncOwnerTransform();

    PxController* controller = nullptr;
    CCHitReport* hitReport = nullptr;
    bool grounded = false;
    bool useGravity = true;
    float verticalVelocity = 0.0f;
    float gravity = -9.81f;
};
