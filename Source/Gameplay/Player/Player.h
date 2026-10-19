// Player.h
#pragma once
#include "Animation/Animator.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "Gameplay/Actor/Entity.h"

#include "Gameplay/Player/PlayerInput.h"
#include "Physics/Collider/CharacterController.h"
#include "Rendering/Component/VMDL.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "Animation/LookAt.h"

class ThirdPersonCameraController;
class CharacterMotorComponent;
class LockOnComponent;
class AfterimageComponent;
class Effect;
class VMDLColliderComponent;

class Player : public Entity
{
  public:
	Player();
	~Player() override;

	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnDrawGUI() override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;
	void OnHeal(float amount) override;
	void TakeDamage(const DamageData& damageData) override;
	bool IsDodgeInvincible() const { return dodgeInvincible; }
	bool IsJustDodging() const { return justDodgeTriggered; }
	bool IsUsingJustDodgeSkill() const { return justDodgeSkillActive; }
	bool IsSprinting() const { return sprinting; }
	// ワールド座標の方向へ回避する。forceでクールダウンと行動制限を無視する（死亡時は不可）。
	void QuickShift(const Vector3& direction, bool force = false);

	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point,
		const Vector3& normal) override;
	void OnTriggerExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point,
		const Vector3& normal) override;

	PlayerInput* GetPlayerInput() const { return GetComponent<PlayerInput>(); }
	ThirdPersonCameraController* GetCameraController() const { return cameraController; }

	VMDLModel* GetModel() const { return model.get(); }

	void SetSpineAngleX(float angleX) { spineAngleX = angleX; }
	float GetSpinAngleX() const { return spineAngleX; }

	void SetFirstPerson(bool firstPerson) { isFirstPerson = firstPerson; }
	bool IsFirstPerson() const { return isFirstPerson; }

	void DeathOfFall() override;

	// ThirdPersonCameraController をセットすることでカメラ基準移動が有効になる
	void SetCameraController(ThirdPersonCameraController* cam) { cameraController = cam; }
	void SetSpawnTransform(const Transform& spawnTransform);

  private:
	struct ActionState
	{
		bool frozen = false;
		bool dodging = false;
		bool skill = false;
		bool attacking = false;
		bool hit = false;
	};

	void InitializeModel();
	void InitializeMovement();
	void RegisterAnimationCallbacks();
	void UpdateAttackContacts();
	bool RegisterAttackContact(PhysicsComponent* attack, PhysicsComponent* other, Actor* target);
	void ResolveAttackHit(
		VMDLColliderComponent* attack, Actor* target, Vector3& hitPosition, Vector3& hitNormal);
	float GetAttackDamage(const Entity* target, bool skillAttack, bool kick) const;
	void HandleAttackContact(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point,
		const Vector3& normal);
	void UpdateTerrainDeformInput();
	ActionState GetActionState() const;
	void UpdateDodgeState(const ActionState& state);
	Vector3 GetCameraRelativeDirection(float inputX, float inputZ) const;
	void UpdateMoveDirection(
		const InputContext& input, float inputLength, const ActionState& state);
	void UpdateMovementAnimation(
		const InputContext& input, float inputLength, const ActionState& state);
	Actor* GetJustDodgeTarget() const;
	void HandleAttackInput(const InputContext& input, const ActionState& state);
	void StartJustDodgeSkill(Actor* target);
	void ActivateJustDodgeSkill();

	struct AttackContact
	{
		std::weak_ptr<Actor> target;
		std::unordered_set<PhysicsComponent*> colliders;
	};
	std::unordered_map<PhysicsComponent*, std::unordered_map<Actor*, AttackContact>> attackContacts;
	void UpdateWaterDamage();
	void UpdateMovement();
	void UpdateHealth();
	void UpdateDeathSequence();
	void UpdateFootSound();
	bool IsEnemyAttackActive(const Actor* enemy, const PhysicsComponent* collider) const;
	void TriggerJustDodge(Actor* attacker);
	void PlayJustDodgeFeedback();
	void PlayAttackHitEffect(const Vector3& position);


  protected:
	std::shared_ptr<VMDLModel> model = nullptr;

	// 描画
	Animator* anim = nullptr;
	int stIdle = -1;
	int stWalk = -1;
	int stRun = -1;
	int stSprint = -1;
	VMDL* vmdl = nullptr;

	ThirdPersonCameraController* cameraController = nullptr;
	bool isFirstPerson = false;
	float spineAngleX = 0.0f;
	const Vector2 idleSpineAngle = {0.8f, 0};
	const Vector2 readySpineAngle = {-0.25f, -0.38f};
	TrailRenderComponent* trail = nullptr;
	AfterimageComponent* afterimage = nullptr;
	std::shared_ptr<Effect> attackHitEffect;
	bool dodgeInvincible = false;
	bool justDodgeTriggered = false;
	bool justDodgeWindowActive = false;
	Actor* justDodgeAttacker = nullptr;
	bool justDodgeSoundPlayed = false;
	bool justDodgeSkillActive = false;
	std::weak_ptr<Actor> justDodgeSkillTarget;
	float dodgeCooldownTimer = 0.0f;
	float dodgeCooldownDuration = 0.35f;

	// 水に触れたらダメージ
	const float damageLowerHeightCoolDown = 0.2f;
	const float damageWaterAmount = 3.0f;
	float damageLowerHeightTimer = 0;
	bool isInWater = false;

	// 移動
	CharacterController* cc = nullptr;
	CharacterMotorComponent* motor = nullptr;
	LockOnComponent* lockOnComponent = nullptr;
	float speed = 5.0f;
	bool sprinting = false;
	bool crouching = false;
	float crouchRootMotionScale = 0.75f;
	float waterMoveSpeedScale = 0.6f; // 水中の歩行・走行速度の倍率（1.0で通常速度）
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