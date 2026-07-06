// Player.h

#pragma once
#include "Animator.h"

#include <memory>
#include <utility>

#include "Entity.h"

#include "PhysicsManager.h"
#include "PlayerController.h"
#include "CharacterController.h"
#include "ModelRenderComponent.h"
#include "BoneSphereCollider.h"
#include "TrailRenderComponent.h"
#include "SpringBone.h"
#include "HumanoidFootIK.h"

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
    void OnDead() override;

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

    void SetController(std::unique_ptr<PlayerController> ctrl) { controller = std::move(ctrl); }
    PlayerController* GetController() const { return controller.get(); }

    Model* GetModel() const { return model.get(); }

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
    void SyncWeaponAttachNodes();

protected:
    std::unique_ptr<PlayerController> controller;
    std::shared_ptr<Model> model = nullptr;

    Animator*             anim = nullptr;
    CharacterController*  cc   = nullptr;
    ModelRenderComponent* modelRenderer = nullptr;
    ThirdPersonCameraController* cameraController = nullptr;
    BoneSphereCollider* weaponCollider = nullptr;
    BoneSphereCollider* footCollider = nullptr;
    TrailRenderComponent* trail = nullptr;
    SpringBone* hairSpringBone = nullptr;
	HumanoidFootIK* footIK = nullptr;
    float groundSnapUpDistance = 0.2f;
    float groundSnapDownDistance = 0.5f;

    bool  isFirstPerson = false;
    float spineAngleX   = 0.0f;
    const Vector2 idleSpineAngle  = {0.8f, 0};
    const Vector2 readySpineAngle = {-0.25f, -0.38f};

    Vector3 frameVelocity = Vector3::Zero;
    float verticalVelocity = 0.0f;
    float speed = 5.0f;
    bool groundedByRay = false;

    ShaderParamListWithMaterialName shaderParamWithMaterialName;

    int stIdle   = -1;
    int stWalk   = -1;
    int stRun    = -1;
    int stSprint = -1;

    Vector3 offsetPos = Vector3::Zero;
};
