#pragma once
#include "Animation/Animator.h"

#include <memory>
#include <unordered_set>

#include "Gameplay/Actor/Entity.h"

#include "Gameplay/Player/PlayerController.h"
#include "Physics/Collider/CharacterController.h"
#include "Rendering/Component/VMDL.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "Animation/LookAt.h"

class ThirdPersonCameraController;
class CharacterMotorComponent;
class LockOnComponent;
class AfterimageComponent;

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
	void TakeDamage(const DamageData& damageData) override;
	bool IsDodgeInvincible() const { return dodgeInvincible; }
	bool IsJustDodging() const { return justDodgeTriggered; }
	bool IsUsingJustDodgeSkill() const { return justDodgeSkillActive; }
	bool IsSprinting() const { return sprinting; }

    void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
    void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
    
    PlayerController* GetController() const { return controller; }
	ThirdPersonCameraController* GetCameraController() const { return cameraController; }

    VMDLModel* GetModel() const { return model.get(); }

    void SetSpineAngleX(float angleX) { spineAngleX = angleX; }
    float GetSpinAngleX() const { return spineAngleX; }

    void SetFirstPerson(bool firstPerson) { isFirstPerson = firstPerson; }
    bool IsFirstPerson() const { return isFirstPerson; }

    // ThirdPersonCameraController をセットすることでカメラ基準移動が有効になる
    void SetCameraController(ThirdPersonCameraController* cam) { cameraController = cam; }
	void SetSpawnTransform(const Transform& spawnTransform);

private:
    void UpdateMovement();
	void UpdateHealth();
	void UpdateDeathSequence();
	void UpdateFootSound();
	bool HasIncomingEnemyAttack() const;
	bool IsEnemyAttackActive(const Actor* enemy, const PhysicsComponent* collider) const;
	void TriggerJustDodge();

protected:
    PlayerController* controller = nullptr;
    std::shared_ptr<VMDLModel> model = nullptr;

    // 描画
    Animator* anim = nullptr;
    int stIdle = -1;
    int stWalk = -1;
    int stRun = -1;
    int stSprint = -1;
    VMDL* vmdl = nullptr;
    
    ThirdPersonCameraController* cameraController = nullptr;
    bool  isFirstPerson = false;
    std::string bufferedQuickStepTrigger;
    float spineAngleX = 0.0f;
    const Vector2 idleSpineAngle = {0.8f, 0};
    const Vector2 readySpineAngle = {-0.25f, -0.38f};
    TrailRenderComponent* trail = nullptr;
	AfterimageComponent* afterimage = nullptr;
	bool dodgeInvincible = false;
	bool justDodgeTriggered = false;
	bool justDodgeSkillActive = false;
	std::unordered_set<Actor*> justDodgeSkillHitActors;
	float dodgeCooldownTimer = 0.0f;
	float dodgeCooldownDuration = 0.35f;

    // 移動
	CharacterController* cc = nullptr;
	CharacterMotorComponent* motor = nullptr;
	LockOnComponent* lockOnComponent = nullptr;
	float speed = 5.0f;
	bool sprinting = false;
	bool crouching = false;
	bool crouchAnimationsAvailable = false;
	float crouchRootMotionScale = 0.55f;
	bool terrainDeformKeyHeld = false;
	bool actionInputThisFrame = false;
	float healthRecoveryTimer = 0.0f;
	float healthRecoveryInterval = 2.0f;
	float healthRecoveryAmount = 5.0f;
	bool deathSequenceActive = false;
	bool deathReloadRequested = false;
	float deathSequenceTimer = 0.0f;
	float deathVignetteDuration = 10.0f;

    // サウンド
    VMDLModel::VmdlSoundSource* footSound = nullptr;
};
