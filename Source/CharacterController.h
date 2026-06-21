// CharacterController.h

#pragma once

#include "Collider.h"
#include "PhysicsManager.h"
#include "Object.h"

class CharacterController : public Collider {
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
    void SetStepOffset(float value);
    void SetSlopeLimitDeg(float value);
    void SetContactOffset(float value);
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
