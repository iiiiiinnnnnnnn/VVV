// EnemyAIFlow.h

#pragma once

#include "Gameplay/AI/AIFlow.h"

class Actor;
class NavMeshAgent;

class EnemyAIFlow : public AIFlow
{
public:
    EnemyAIFlow(Object* owner);

    void OnDisabled() override;
    void OnRender(const RenderContext& rc) override;
    const char* GetDebugName() const override { return ICON_FA_COG " EnemyAIFlow"; }

    void CreateDefaultChaseGraph();
    void SetTargetTag(const std::string& value) { targetTag = value; }
    void SetSearchRange(float value);
    void SetAgentRadius(float value) { agentRadius = value; }
    Actor* GetTarget() const { return target; }
    void LockOn(Actor* actor);
    void SetMovementLocked(bool value);
    void MoveToTarget(float speed);
    void StopMovement();

protected:
    void UpdateBlackboard() override;
    void DrawFlowInspector() override;
    bool LoadFlowExtension(const std::string& path) override;
    bool SaveFlowExtension(const std::string& path) const override;

private:
    void UpdateSightRay();
    void FaceTarget();

    std::string targetTag = "Player";
    Actor* target = nullptr;
    Actor* lockedTarget = nullptr;
    NavMeshAgent* navMeshAgent = nullptr;
    float agentRadius = 1.0f;
    float trackingTurnSpeed = 8.0f;
    Vector3 sightRayStart = Vector3::Zero;
    Vector3 sightRayEnd = Vector3::Zero;
    bool sightRayHit = false;
	bool movementLocked = false;
	bool showSightDebug = true;
};
