// CharacterController.h

#pragma once

#include "Physics/Core/PhysicsComponent.h"
#include "Physics/Core/PhysicsManager.h"
#include "Core/Object/Object.h"

class CharacterController : public PhysicsComponent {
public:
    CharacterController(Object* owner, LayerId layerId, float radius, float height);
    ~CharacterController();
    void Update() override;
    void LateUpdate() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_STREET_VIEW " CharacterController"; }
    void Move(const Vector3& velocity);
    void SetPosition(const Vector3& pos);
    void SetFootPosition(const Vector3& pos);
    void SetOwnerAnchorOffsetY(float value);
    void SetOwnerAnchorAtCenter(bool value);
    void SetDebugRenderPosition(const Vector3& position);
    void ClearDebugRenderPosition();
    void SetUseGravity(bool value) { useGravity = value; }
    void SetPushable(bool value) { pushable = value; }
    bool IsPushable() const { return pushable; }
    void SetStepOffset(float value);
    void SetConstrainedClimbing(bool value)
    {
        if (!controller) return;

        PxCapsuleController* capsule = static_cast<PxCapsuleController*>(controller);
        capsule->setClimbingMode(
            value
            ? PxCapsuleClimbingMode::eCONSTRAINED
            : PxCapsuleClimbingMode::eEASY);
    }
    void SetSlopeLimitDeg(float value);
    void SetContactOffset(float value);
    void ReleaseController();
    bool IsGrounded() const { return grounded; }
private:
    void ApplyGravity();
    void SyncOwnerTransform();
    float GetFootToControllerCenter() const;

    PxController* controller = nullptr;
    CCHitReport* hitReport = nullptr;
    bool grounded = false;
    bool useGravity = true;
    bool pushable = true;
    float verticalVelocity = 0.0f;
    float gravity = -9.81f;
    float ownerAnchorOffsetY = 0.0f;
    bool ownerAnchorAtCenter = false;
    Vector3 debugRenderPosition = Vector3::Zero;
    bool useDebugRenderPosition = false;
};
