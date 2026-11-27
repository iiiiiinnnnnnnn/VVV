#pragma once
#include <string>
#include <vector>

#include "Core/Object/Component.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "Physics/Navigation/NavMeshBuilder.h"

class ActorManager;
class Terrain;
struct RenderContext;

class NavMeshActor : public Component
{
  public:
	void OnAwake() override;
	NavMeshActor(Object* owner);
	~NavMeshActor() override;

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MAP " ナビメッシュ生成"; }

	bool FindNextPoint(const Vector3& start, const Vector3& goal, Vector3& nextPoint,
		Vector3* reachableGoal = nullptr) const;
	bool FindNearestPoint(const Vector3& position, Vector3& nearestPoint) const;
	bool IsOutsideOrNearBoundary(const Vector3& position, float distance) const;
	bool FindRecoveryPoint(
		const Vector3& position, float safeDistance, Vector3& recoveryPoint) const;
	bool FindRandomPoint(
		const Vector3& center, float minDistance, float maxDistance, Vector3& randomPoint) const;
	bool FindObstacleDetourPoint(
		const Vector3& start, const Vector3& goal, Vector3& nextPoint) const;
	bool IsCliffAlongSegment(const Vector3& start, const Vector3& goal, float maxSlope) const;
	bool IsDirectPathBlocked(const Vector3& start, const Vector3& goal) const;

	void RequestBuild(int delayFrames = 1);
	void RequestBuildRegion(const Vector3& center, float radius, int delayFrames = 1);
	std::string SaveSettingsJson() const;
	bool LoadSettingsJson(const std::string& text);

	static NavMeshActor* GetActive() { return active; }

  private:
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
		NavMeshBuilder::BuildStats stats;
		int inputWalkableCells = 0;
	};

	void Build();
	bool BuildTile(int tileX, int tileZ, Terrain& terrain);
	void RebuildRegion();
	void RefreshDebugCells();
	void Release();
	Vector3 ProjectTerrainPosition(const Vector3& position, bool navigationBase) const;

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
	float buildMinY = -100.0f;
	float buildMaxY = 100.0f;
	std::string buildDiagnostics;
	Vector3 rebuildRegionMin = Vector3::Zero;
	Vector3 rebuildRegionMax = Vector3::Zero;
};
