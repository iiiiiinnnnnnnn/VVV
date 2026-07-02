// NavMeshActor.h

#pragma once
#include <string>
#include <vector>

#include "Common.h"
#include "Component.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"

class ActorManager;
class Terrain;
struct RenderContext;

class NavMeshActor : public Component
{
public:
	NavMeshActor(Object* owner);
	~NavMeshActor() override;

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MAP " NavMeshActor"; }

	bool FindNextPoint(
		const Vector3& start,
		const Vector3& goal,
		Vector3& nextPoint) const;

	void RequestBuild(int delayFrames = 1);
	void SetAgentRadius(float value);
	float GetAgentRadius() const { return agentRadius; }

	static NavMeshActor* GetActive() { return active; }

private:
	struct ObstacleBounds
	{
		Vector3 center = Vector3::Zero;
		Vector3 size = Vector3::Zero;
	};

	struct DebugCell
	{
		Vector3 corners[3] = {};
		bool walkable = false;
	};

	void Build();
	void Release();
	void CollectObstacles(std::vector<ObstacleBounds>& obstacles) const;
	bool IsBlockedByObstacle(
		const Vector3& center,
		const std::vector<ObstacleBounds>& obstacles) const;

	static NavMeshActor* active;

	dtNavMesh* navMesh = nullptr;
	dtNavMeshQuery* navQuery = nullptr;

	bool buildRequested = true;
	bool built = false;
	int buildDelayFrames = 1;
	int resolution = 128;
	int debugDrawStep = 2;
	float agentHeight = 2.0f;
	float agentRadius = 0.6f;
	float agentClimb = 1.2f;
	float nearestPolyExtent = 8.0f;
	float navMinY = -100.0f;
	float navMaxY = 100.0f;
	std::string statusMessage;
	std::vector<DebugCell> debugCells;
};
