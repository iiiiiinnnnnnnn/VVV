#pragma once
#include "Rendering/Core/VMatRenderParams.h"
#include <memory>
#include <vector>

#include "Gameplay/Actor/Entity.h"
#include "Gameplay/AI/EnemyAIFlow.h"

class AracoreQueen;
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

class AracoreQueen : public Entity
{
public:
	AracoreQueen(Player* player_init, Vector3 position, Terrain* terrain_init = nullptr);
	~AracoreQueen() override;
	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnDrawGUI() override;

	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnCollisionStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;
	bool IsChasingPlayer() const { return chaseBgmEngaged; }
	bool HasDetectedPlayer() const { return playerDetected; }
	void SetBossBar(BossBar* value);
	void SetStageLoader(StageLoader* value) { stageLoader = value; }

private:
	// 威嚇モーションと画面演出をまとめて開始する。
	void PlayThreatPresentation();
	// AIの対象取得を監視し、Threat状態の開始通知を取りこぼしても演出する。
	void UpdateThreatPresentation();
	void UpdateAnimatedModelTransform();
	void DeformTerrainAtLanding();
	void SpawnDeerFromSky();
	void ShakeCameraAtLanding();
	void RequestAnimatedMovement(float speed, const char* requiredAnimation);
	void StopAnimatedMovement();
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
	bool landingDeformPending = false;
	bool deathSequenceActive = false;
	bool deathAnimationStarted = false;
	bool deathThreatStarted = false;
	float deathSequenceTimer = 0.0f;
	float deathJumpDuration = 1.8f;
	float deathJumpHeight = 9.0f;
	float deathThreatDuration = 2.2f;
	float requestedAnimationMoveSpeed = 0.0f;
	std::string requiredMovementAnimation;
	bool movementAnimationGateActive = false;
	Player* player;
	Actor* threatenedTarget = nullptr;
	uint64_t voiceIdChase = 0;
	bool chaseBgmEngaged = false;
	bool playerDetected = false;
	BossBar* bossBar = nullptr;
	float chaseBgmVolume = 0.0f;
	float chaseBgmFadeInSeconds = 1.0f;
	float chaseBgmFadeOutSeconds = 1.5f;

	VMatRenderParams renderParams;
};
