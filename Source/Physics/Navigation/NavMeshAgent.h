// NavMeshAgent.h

#pragma once
#include <string>

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"

class Actor;
class CharacterController;

class NavMeshAgent : public Component
{
public:
	NavMeshAgent(Object* owner);

	void Update() override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MAP " NavMeshAgent"; }

	void SetTarget(Actor* target) { this->target = target; }
	void SetTargetTag(const std::string& tag) { targetTag = tag; }
	void SetAutoMove(bool enabled) { autoMove = enabled; }
	void SetMovementPaused(bool value) { movementPaused = value; }
	void MoveToTarget(Actor* targetActor);
	void MoveToPosition(const Vector3& targetPosition);
	bool MoveToRandomPosition(float minDistance, float maxDistance);
	void Stop();

	void SetSpeed(float value) { speed = value; }
	void SetTurnSpeed(float value) { turnSpeed = std::max(value, 0.0f); }
	void SetStoppingDistance(float value) { stoppingDistance = std::max(value, 0.0f); }
	float GetSpeed() const { return speed; }
	float GetStoppingDistance() const { return stoppingDistance; }
	float GetMoveAmount() const { return currentSpeed; }
	float GetTurnAngle() const { return currentTurnAngle; }
	const Vector3& GetMoveDelta() const { return lastMoveDelta; }
	const Vector3& GetDestination() const { return destination; }
	bool HasDestination() const { return hasDestination; }
	const std::string& GetStatusMessage() const { return statusMessage; }

private:
	Actor* FindTargetByTag();
	bool MoveToPosition(Actor* actor, const Vector3& targetPosition);

	Actor* target = nullptr;
	CharacterController* characterController = nullptr;
	std::string targetTag = "Player";
	std::string statusMessage;
	bool autoMove = false;
	bool hasDestination = false;
	bool chaseTargetTag = false;
	bool rotateToMoveDirection = true;
	bool movementPaused = false;
	bool useLastValidPathOnFail = true;
	bool directMoveOnPathFail = true;
	bool hasLastNextPoint = false;
	Vector3 lastNextPoint = Vector3::Zero;
	Vector3 destination = Vector3::Zero;
	float randomMinDistance = 3.0f;
	float randomMaxDistance = 10.0f;
	float stoppingDistance = 1.5f;
	float pathFailGraceTime = 0.5f;
	float pathFailTimer = 0.0f;
	float turnSpeed = 8.0f;
	float currentTurnAngle = 0.0f;
	Vector3 lastMoveDelta = Vector3::Zero;
	float currentSpeed = 0.0f;
	float speed = 3.5f;
};
