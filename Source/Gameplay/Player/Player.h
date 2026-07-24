// Player.h

#pragma once
#include "Animation/Animator.h"

#include <memory>

#include "Gameplay/Actor/Entity.h"

#include "Physics/Core/PhysicsManager.h"
#include "Gameplay/Player/PlayerController.h"
#include "Physics/Collider/CharacterController.h"
#include "Rendering/Component/VMDL.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "Animation/LookAt.h"

class ThirdPersonCameraController;

class Player : public Entity
{
public:
    Player();
    ~Player() override = default;

    void OnUpdate() override;
    void OnLateUpdate() override;
    void OnDrawGUI() override;

    void OnDamaged(const DamageData& damageData) override;
    void OnDead(const DamageData& damageData) override;

    void OnEnterAnim(const Animator::State& state);
    void OnExitAnim(const Animator::State& state);
    void OnEnterAnimAttack4B(const Animator::State& state);
    void OnExitAnimAttack4B(const Animator::State& state);

    void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
    void OnCollisionStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
    void OnCollisionExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
    void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
    void OnTriggerStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
    void OnTriggerExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

    PlayerController* GetController() const { return controller; }

    VMDLModel* GetModel() const { return model.get(); }

    void SetSpineAngleX(float angleX) { spineAngleX = angleX; }
    float GetSpinAngleX() const { return spineAngleX; }

    void SetFirstPerson(bool firstPerson) { isFirstPerson = firstPerson; }
    bool IsFirstPerson() const { return isFirstPerson; }

    // ThirdPersonCameraController をセットすることでカメラ基準移動が有効になる
    void SetCameraController(ThirdPersonCameraController* cam) { cameraController = cam; }

private:
    Matrix GetModelWorldTransform() const;
    bool RaycastGround(PhysicsManager::PhysicsRaycastHit& hit) const;
    void SnapToGroundIfNeeded();
    void UpdateLookIn();
    void UpdateMovement();

protected:
    PlayerController* controller = nullptr;
    std::shared_ptr<VMDLModel> model = nullptr;

    // rendering
    Animator* anim = nullptr;
    int stIdle = -1;
    int stWalk = -1;
    int stRun = -1;
    int stSprint = -1;
    VMDL* vmdl = nullptr;
    ThirdPersonCameraController* cameraController = nullptr;
    bool  isFirstPerson = false;
    float spineAngleX = 0.0f;
    const Vector2 idleSpineAngle = {0.8f, 0};
    const Vector2 readySpineAngle = {-0.25f, -0.38f};
    TrailRenderComponent* trail = nullptr;
    float groundSnapUpDistance = 0.2f;
    float groundSnapDownDistance = 0.5f;

    // look
    Actor* lookInTarget = nullptr;
    LookAt* lookAt = nullptr;

    // movement
    CharacterController* cc = nullptr;
    VMDLColliderComponent* weaponCollider = nullptr;
    VMDLColliderComponent* footCollider = nullptr;
    Vector3 frameVelocity = Vector3::Zero;
    float verticalVelocity = 0.0f;
    float speed = 5.0f;
    bool groundedByRay = false;
};
