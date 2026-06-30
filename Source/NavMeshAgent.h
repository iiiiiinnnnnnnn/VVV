// NavMeshAgent.h

#pragma once

#include "Common.h"
#include "Component.h"

class Actor;
class CharacterController;

class NavMeshAgent : public Component
{
public:
	NavMeshAgent(Object* owner);

	void Update() override;
	void DrawGUI() override;

	void SetTarget(Actor* target) { this->target = target; }
	void SetTargetTag(const std::string& tag) { targetTag = tag; }
	void SetAutoMove(bool enabled) { autoMove = enabled; }
	void MoveToTarget(Actor* targetActor);
	void Stop();

	float GetSpeed() const { return speed; }
	float GetMoveAmount() const { return lastMoveDelta.Length(); }
	const Vector3& GetMoveDelta() const { return lastMoveDelta; }

private:
	Actor* FindTargetByTag();
	void MoveToTarget(Actor* actor, Actor* targetActor);

	Actor* target = nullptr;
	CharacterController* characterController = nullptr;
	std::string targetTag = "Player";
	std::string statusMessage;
	bool autoMove = false;
	bool chaseTargetTag = false;
	bool rotateToMoveDirection = true;
	bool useLastValidPathOnFail = true;
	bool hasLastNextPoint = false;
	Vector3 lastNextPoint = Vector3::Zero;
	Vector3 lastMoveDelta = Vector3::Zero;
	float speed = 3.5f;
	float stoppingDistance = 1.5f;
	float repathDistance = 1.0f;
	float pathFailGraceTime = 0.5f;
	float pathFailTimer = 0.0f;
	float turnSpeed = 8.0f;
};
