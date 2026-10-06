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
	NavMeshActor(Object* owner);
	~NavMeshActor() override;

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MAP " NavMeshActor"; }

	bool FindNextPoint(const Vector3& start, const Vector3& goal, Vector3& nextPoint,
		Vector3* reachableGoal = nullptr, float radius = 0.0f, float height = 0.0f) const;
	bool FindNearestPoint(const Vector3& position, Vector3& nearestPoint) const;
	bool IsOutsideOrNearBoundary(const Vector3& position, float distance) const;
	bool FindRecoveryPoint(
		const Vector3& position, float safeDistance, Vector3& recoveryPoint) const;
	bool FindRandomPoint(
		const Vector3& center, float minDistance, float maxDistance, Vector3& randomPoint) const;
	bool FindObstacleDetourPoint(
		const Vector3& start, const Vector3& goal, Vector3& nextPoint,
		float radius = 0.0f, float height = 0.0f) const;
	bool IsCliffAlongSegment(const Vector3& start, const Vector3& goal, float maxSlope) const;
	bool IsDirectPathBlocked(const Vector3& start, const Vector3& goal,
		float radius = 0.0f, float height = 0.0f) const;

	void RequestBuild(int delayFrames = 1);
	void RequestBuildRegion(const Vector3& center, float radius, int delayFrames = 1);
	std::string SaveSettingsJson() const;
	bool LoadSettingsJson(const std::string& text);

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

	// 動的地形用NavMeshタイル

	static constexpr int CellsPerTile = 32;

	struct NavTile
	{
		dtTileRef reference = 0;
		std::vector<DebugCell> debugCells;
	};

	void Build();
	bool BuildTile(
		int tileX,
		int tileZ,
		Terrain& terrain);
	void RebuildRegion();
	void RefreshDebugCells();
	void Release();
	void CollectObstacles(std::vector<ObstacleBounds>& obstacles) const;

	static NavMeshActor* active;

	dtNavMesh* navMesh = nullptr;
	dtNavMeshQuery* navQuery = nullptr;

	bool buildRequested = true;
	bool regionBuildRequested = false;
	bool built = false;
	int buildDelayFrames = 1;
	int tileCountX = 0;
	int tileCountZ = 0;
	bool simplifyFlatAreas = true;
	float simplifyHeightError = 0.02f;
	int simplifyMaxCells = 8;
	static constexpr float nearestPolyExtent = 8.0f;
	float navMinY = -100.0f;
	float navMaxY = 100.0f;
	std::string statusMessage;
	std::vector<DebugCell> debugCells;
	std::vector<NavTile> navTiles;
	Vector3 rebuildRegionMin = Vector3::Zero;
	Vector3 rebuildRegionMax = Vector3::Zero;
};
