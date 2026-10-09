#pragma once
#include "Rendering/Core/VMatRenderParams.h"
#include <memory>
#include <string>
#include <vector>

#include "Gameplay/Actor/Entity.h"
#include "Gameplay/AI/EnemyAIFlow.h"

class Aracore;
class VMDL;
class CharacterController;
class Player;
class Terrain;
class BossBar;
class StageLoader;

// component
#include "Physics/RigidBody/RigidbodyDynamic.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Animation/Animator.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Physics/Core/PhysicsComponent.h"
class MultiLegFootIK;

class Aracore : public Entity
{
public:
	Aracore(Player* player_init, const Transform& transform, Terrain* terrain_init = nullptr,
		const std::string& modelPath = "Resources/Model/Aracore");
	~Aracore() override;
	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnDrawGUI() override;
	void TakeDamage(const DamageData& damageData) override;

	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnCollisionStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;
	bool IsChasingPlayer() const { return chaseBgmEngaged; }
	bool HasDetectedPlayer() const { return playerDetected; }
	void SetBossBar(BossBar* value);
	void SetStageLoader(StageLoader* value) { stageLoader = value; }

	bool IsThreating() const { return phaseThreatActive; }

private:
	// 威嚇モーションと画面演出をまとめて開始する。
	void PlayThreatPresentation();
	void BeginPhaseChange();
	void SpawnPhaseFragments();
	void UpdateHealthPresentation();
	void UpdateHealthDissolve(float deltaTime);
	void UpdatePhaseColor();
	void ApplyEnragedAIParameters();
	void ConfigureAI();
	void RegisterAICallbacks();
	int GetJumpSequenceCount() const;
	// AIの対象取得を監視し、Threat状態の開始通知を取りこぼしても演出する。
	void UpdateThreatPresentation();
	void UpdateReactionAnimation();
	void UpdateAnimatedModelTransform();
	void DeformTerrainAtLanding();
	void SpawnDeerFromSky();
	void ShakeCameraAtLanding();
	void RequestAnimatedMovement(float speed, const char* requiredAnimation);
	void StopAnimatedMovement();
	void UpdateJumpAnimationLandingHold();
	void UpdateChaseBgm();
	void StopChaseBgm();
	void SetPlayerDetected(bool detected);
	void UpdateDeathSequence();
	bool IsMovementAnimationReady() const;
	bool PushPlayer(PhysicsComponent* self, PhysicsComponent* other);

	Animator* anim = nullptr;
	VMDL* vmdl = nullptr;
	std::shared_ptr<VMDLModel> model;
	CharacterController* characterController = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	MultiLegFootIK* multiLegFootIK = nullptr;
	EnemyAIFlow* controller = nullptr;
	std::vector<Vector3> colPositions;
	Vector3 jumpStartPosition = Vector3::Zero;
	Vector3 jumpLandingPosition = Vector3::Zero;
	Vector3 spawnPosition = Vector3::Zero;
	Vector3 deathJumpStartPosition = Vector3::Zero;
	Terrain* terrain = nullptr;
	StageLoader* stageLoader = nullptr;
	bool jumpLanded = false;
	bool jumpAnimationPending = false;
	bool jumpAnimationWaitingForLanding = false;
	bool landingDeformPending = false;
	bool deathSequenceActive = false;
	bool deathAnimationStarted = false;
	float deathSequenceTimer = 0.0f;
	float deathJumpDuration = 1.8f;
	float deathJumpHeight = 9.0f;
	float requestedAnimationMoveSpeed = 0.0f;
	std::string requiredMovementAnimation;
	bool movementAnimationGateActive = false;
	Player* player;
	Actor* threatenedTarget = nullptr;
	uint64_t voiceIdChase = 0;
	bool chaseBgmEngaged = false;
	bool playerDetected = false;
	bool damageAnimationPending = false;
	bool threatAnimationPending = false;
	BossBar* bossBar = nullptr;
	float chaseBgmVolume = 0.0f;
	float chaseBgmFadeInSeconds = 1.0f;
	float chaseBgmFadeOutSeconds = 1.5f;

	bool enraged = false;
	bool phaseChangeQueued = false;
	bool phasePresentationPending = false;
	bool phaseThreatActive = false;
	float phaseThreatElapsed = 0.0f;
	float phaseColorElapsed = 0.0f;
	float healthDissolveAmount = 0.0f;
	int remainingJumps = 0;
	std::shared_ptr<VMDLModel> phaseFragmentModel;
	Vector3 phaseFragmentSize = Vector3::One;

	VMatRenderParams renderParams;
};
