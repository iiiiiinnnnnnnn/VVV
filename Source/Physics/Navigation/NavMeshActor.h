// NavMeshActor.h

#pragma once
#include <string>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"

class ActorManager;
class Terrain;
struct RenderContext;

class NavMeshActor : public Component
{
public:
	struct WalkableArea
	{
		Vector3 center = Vector3::Zero;
		Vector3 size = Vector3::One;
	};

	NavMeshActor(Object* owner);
	~NavMeshActor() override;

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MAP " NavMeshActor"; }

	bool FindNextPoint(
		const Vector3& start,
		const Vector3& goal,
		Vector3& nextPoint,
		Vector3* reachableGoal = nullptr) const;
	bool FindNearestPoint(
		const Vector3& position,
		Vector3& nearestPoint) const;
	bool FindRecoveryPoint(
		const Vector3& position,
		float safeDistance,
		Vector3& recoveryPoint) const;
	bool FindRandomPoint(
		const Vector3& center,
		float minDistance,
		float maxDistance,
		Vector3& randomPoint) const;
	bool FindObstacleDetourPoint(
		const Vector3& start,
		const Vector3& goal,
		Vector3& nextPoint) const;
	bool IsDirectPathBlocked(const Vector3& start, const Vector3& goal) const;

	void RequestBuild(int delayFrames = 1);
	void SetAgentRadius(float value);
	void SetResolution(int value);
	void AddWalkableArea(const Vector3& center, const Vector3& size);
	void ClearWalkableAreas();
	std::string SaveSettingsJson() const;
	bool LoadSettingsJson(const std::string& text);
	float GetAgentRadius() const { return agentRadius; }
	int GetResolution() const { return resolution; }
	const std::vector<WalkableArea>& GetWalkableAreas() const { return walkableAreas; }

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
		const std::vector<ObstacleBounds>& obstacles,
		float cellHalfSize) const;
	bool IsInsideWalkableArea(const Vector3& center, float cellHalfSize) const;
	bool IsSegmentInsideWalkableAreas(
		const Vector3& start,
		const Vector3& goal) const;

	static NavMeshActor* active;

	dtNavMesh* navMesh = nullptr;
	dtNavMeshQuery* navQuery = nullptr;

	bool buildRequested = true;
	bool built = false;
	bool showWalkableCells = true;
	bool showBlockedCells = true;
	bool showObstacleBounds = true;
	bool showWalkableAreaBounds = true;
	bool showNavMeshDebug = true;
	int buildDelayFrames = 1;
	int resolution = 128;
	int debugDrawStep = 1;
	float agentHeight = 2.0f;
	float agentRadius = 0.6f;
	float agentClimb = 1.2f;
	float agentMaxSlope = 70.0f;
	float nearestPolyExtent = 8.0f;
	float navMinY = -100.0f;
	float navMaxY = 100.0f;
	std::string statusMessage;
	std::vector<DebugCell> debugCells;
	std::vector<WalkableArea> walkableAreas;
};
