#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Gameplay/Actor/Entity.h"
#include "Application/Time/Timer.h"

class Animator;
class BossBar;
class CharacterController;
class EnemyAI;
class MultiLegFootIK;
class NavMeshAgent;
class Player;
class StageLoader;
class Terrain;
class VMDL;
class VMDLModel;

// Luaが選んだ移動を、移動・旋回アニメーションと物理移動へ反映する。
// 行動の選択や攻撃タイミングは担当しない。
class AracoreAINavigation
{
public:
	void Initialize(EnemyAI* ai, NavMeshAgent* agent, Animator* animator, MultiLegFootIK* feet);

	void Walk(float speed) { Request(Movement::Walk, speed); }
	void Chase(float speed) { Request(Movement::Chase, speed); }
	void TurnInPlace() { Request(Movement::TurnInPlace, 0.0f); }
	void Charge(float speed) { Request(Movement::Charge, speed); }
	void Stop();
	void Update();

private:
	enum class Movement { Stopped, Walk, Chase, TurnInPlace, Charge };

	void Request(Movement movement, float speed);
	void UpdateTurnDirection(float angle);
	bool IsAnimationReady() const;

	EnemyAI* controller = nullptr;
	NavMeshAgent* agent = nullptr;
	Animator* animator = nullptr;
	MultiLegFootIK* feet = nullptr;

	Movement movement = Movement::Stopped;
	float speed = 0.0f;
	int turningDirection = 0;
};

class Aracore : public Entity
{
public:
	Aracore(Player* player, const Transform& transform, Terrain* terrain = nullptr,
		const std::string& modelPath = "Aracore");
	~Aracore() override;

	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnDrawGUI() override;

	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other,
		const Vector3& point, const Vector3& normal) override;
	void OnCollisionStay(PhysicsComponent* self, PhysicsComponent* other,
		const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other,
		const Vector3& point, const Vector3& normal) override;

	void TakeDamage(const DamageData& damageData) override;
	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;

	bool IsChasingPlayer() const { return chaseAudio.IsEngaged(); }
	bool IsBattleBgmPlaying() const { return chaseAudio.IsPlaying(); }
	bool HasDetectedPlayer() const { return playerDetected; }
	bool IsThreating() const { return phase.threatActive; }
	void SetBossBar(BossBar* value);
	void SetStageLoader(StageLoader* value) { stageLoader = value; }

private:
	// ジャンプの移動・アニメーション待機・着地後の処理。
	struct JumpState
	{
		Vector3 startPosition = Vector3::Zero;
		Vector3 landingPosition = Vector3::Zero;
		bool landed = false;
		bool animationPending = false;
		bool waitingForLanding = false;
		bool deformPending = false;
	};

	// 形態変化の開始、Threat再生の確認、演出時間をひとまとまりに扱う。
	struct PhaseState
	{
		void Begin()
		{
			angry = true;
			threatActive = true;
			waitingForAnimation = true;
			elapsed.Reset();
		}

		void AnimationStarted()
		{
			waitingForAnimation = false;
			elapsed.Reset();
		}

		void Advance(float deltaTime)
		{
			if (angry && !waitingForAnimation && threatActive) elapsed.Advance(deltaTime);
		}

		bool angry = false;
		bool threatActive = false;
		bool waitingForAnimation = false;
		Game::Timer elapsed{0.0f};
		std::shared_ptr<VMDLModel> fragmentModel;
		Vector3 fragmentSize = Vector3::One;
	};

	// Animatorが要求を受理するまで保持する。優先順位は被ダメージ→ジャンプ→威嚇。
	struct ReactionState
	{
		void RequestDamage() { damagePending = true; }
		void RequestThreat() { threatPending = true; }

		bool damagePending = false;
		bool threatPending = false;
		Actor* lastThreatTarget = nullptr;
	};

	// 帰還ジャンプと死亡アニメーションは同時に進めない。
	struct DeathState
	{
		enum class Stage { Inactive, Returning, Animation };

		void Begin(const Vector3& start, const Vector3& landing)
		{
			stage = Stage::Returning;
			elapsed.Reset();
			startPosition = start;
			landingPosition = landing;
		}

		void Landed() { stage = Stage::Animation; }

		bool IsReturning() const { return stage == Stage::Returning; }
		bool IsAnimationPlaying() const { return stage == Stage::Animation; }

		Stage stage = Stage::Inactive;
		Vector3 startPosition = Vector3::Zero;
		Vector3 landingPosition = Vector3::Zero;
		Game::Timer elapsed{0.0f};
		float duration = 1.8f;
		float height = 9.0f;
	};

	// 追跡の有無と、再生中BGMのフェードを管理する。
	class ChaseAudio
	{
	public:
		void Engage() { engaged = true; }
		void Disengage() { engaged = false; }
		bool IsEngaged() const { return engaged; }
		bool IsPlaying() const;
		void Update();
		void Stop();

		float fadeInSeconds = 1.0f;
		float fadeOutSeconds = 1.5f;

	private:
		uint64_t voiceId = 0;
		float volume = 0.0f;
		bool engaged = false;
	};

	Game::Timer healTime{10.0f};

	// 初期化とLuaへの行動登録。
	void OnInit() override;
	void OnUninit() override;
	void ConfigureAI();
	void RegisterAIActions();

	// Luaから呼ばれる行動。開始・更新・終了を同じ順に並べる。
	void StopAIAction();
	void BeginFreedomWait();
	void BeginWander();
	void UpdateWanderTurn();
	void EndWanderTurn();
	void BeginThreat();
	void BeginChase();
	void UpdateChase();
	void BeginChargeTurn();
	void UpdateChargeTurn();
	void BeginCharge();
	void UpdateCharge();
	void BeginRecovery();
	void BeginJump();
	void UpdateJump();
	void EndJump();
	void BeginPhaseThreat();
	void UpdatePhaseThreat();
	void EndPhaseThreat();

	// アニメーション要求の再送と、ジャンプの着地待ち。
	void PlayThreatPresentation();
	void UpdateThreatPresentation();
	void UpdateReactionAnimation();
	void UpdateJumpAnimationLandingHold();
	void UpdateAnimatedModelTransform();

	// 形態変化・体力表示・着地演出。
	void BeginPhaseChange();
	void SpawnPhaseFragments();
	void UpdateHealthPresentation();
	void UpdateHealthDissolve(float deltaTime);
	void UpdateAngryColor();
	void DeformTerrainAtLanding();
	void SpawnDeerFromSky();
	void ShakeCameraAtLanding();
	void UpdateDeathSequence();

	// 接触・追跡BGM・HUD。
	bool PushPlayer(PhysicsComponent* self, PhysicsComponent* other);
	void UpdateChaseBgm();
	void SetPlayerDetected(bool detected);

	// コンポーネントの所有権はEntity側にある。
	std::string modelPath;
	Animator* anim = nullptr;
	VMDL* vmdl = nullptr;
	std::shared_ptr<VMDLModel> model;
	CharacterController* characterController = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	MultiLegFootIK* multiLegFootIK = nullptr;
	EnemyAI* controller = nullptr;

	AracoreAINavigation navigation;
	JumpState jump;
	PhaseState phase;
	ReactionState reaction;
	DeathState death;
	ChaseAudio chaseAudio;

	// シーン側から渡される参照と、ボス全体の情報。
	Player* player = nullptr;
	Terrain* terrain = nullptr;
	StageLoader* stageLoader = nullptr;
	BossBar* bossBar = nullptr;
	Vector3 spawnPosition = Vector3::Zero;
	bool playerDetected = false;
	float healthDissolveAmount = 0.0f;
	std::vector<Vector3> colPositions;
};
