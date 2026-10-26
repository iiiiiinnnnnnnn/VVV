#pragma once

#include "Gameplay/AI/LuaScript.h"
#include "Core/Foundation/Common.h"
#include <functional>
#include <unordered_map>

class Actor;
class ActorManager;
class NavMeshAgent;

class EnemyAI : public LuaScript
{
public:
	// LuaはResources/AIと拡張子を省略できる。省略時は後からLoadで指定する。
	EnemyAI(Object* owner, const std::string& path = {});

	void OnStart() override;
	void OnUpdate() override;
	// 初期化時に指定すると、一度カメラに映るまでLuaの開始・更新を待つ。
	void WaitForCamera(const Vector3& centerOffset, float radius);
	bool IsWaitingForCamera() const { return waitingForCamera; }

	void OnDisabled() override;
	void OnRender(const RenderContext& rc) override;
	const char* GetDebugName() const override { return ICON_FA_COG " EnemyAI"; }

	using Action = std::function<void()>;
	void AddAction(const std::string& name, Action enter = {}, Action update = {}, Action late = {}, Action exit = {});
	void SetBool(const std::string& name, bool value) { bools[name] = value; }
	void SetFloat(const std::string& name, float value) { floats[name] = value; }
	void SetVector3(const std::string& name, const Vector3& value) { vectors[name] = value; }
	bool GetBool(const std::string& name, bool fallback = false) const;
	float GetFloat(const std::string& name, float fallback = 0.0f) const;
	void OnEnabled() override;
	void OnLateUpdate() override;
	void OnDrawGUI() override;
	void SetTargetTag(const std::string& value) { targetTag = value; }
	void SetSearchRange(float value);
	void SetLostRange(float value);
	void SetTrackingTurnSpeed(float value) { trackingTurnSpeed = std::max(value, 0.0f); }
	Actor* GetTarget() const { return target; }
	void LockOn(Actor* actor);
	void SetMovementLocked(bool value);
	void MoveToTarget(float speed);
	void FaceTarget();
	void StopMovement();

protected:
	void UpdateSensors() override;
	void BindEnvironment(sol::environment& environment) override;

private:
	float UpdateTarget(Actor* ownerActor, ActorManager* actorManager, float searchRange, float lostRange);
	void UpdateTurnSensors(bool hasTarget);
	void UpdateSightRay();
	bool IsVisibleToCamera() const;

	std::unordered_map<std::string, bool> bools;
	std::unordered_map<std::string, float> floats;
	std::unordered_map<std::string, Vector3> vectors;
	std::unordered_map<std::string, std::vector<Action>> actions;
	std::string stateName;
	std::string targetTag = "Player";
	Actor* target = nullptr;
	Actor* lockedTarget = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	float trackingTurnSpeed = 8.0f;
	Vector3 sightRayStart = Vector3::Zero;
	Vector3 sightRayEnd = Vector3::Zero;
	bool sightRayHit = false;
	bool movementLocked = false;
	bool showSightDebug = true;
	bool waitingForCamera = false;
	Vector3 cameraActivationCenter = Vector3::Zero;
	float cameraActivationRadius = 0.0f;
};
