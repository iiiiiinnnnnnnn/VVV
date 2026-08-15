#include "Physics/Navigation/NavMeshActor.h"

#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Rendering/Core/Graphics.h"
#include "Physics/Navigation/NavMeshObstacle.h"
#include "Rendering/Renderer/PrimitiveRenderer.h"
#include "Rendering/Core/RenderContext.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Core/Foundation/Json.h"

#include "DetourAlloc.h"
#include "DetourNavMeshBuilder.h"
#include "DetourStatus.h"

#include <map>

NavMeshActor* NavMeshActor::active = nullptr;

NavMeshActor::NavMeshActor(Object* owner) : Component(owner)
{
	active = this;
}

NavMeshActor::~NavMeshActor()
{
	if (active == this) active = nullptr;

	Release();
}

void NavMeshActor::Release()
{
	if (navQuery)
	{
		dtFreeNavMeshQuery(navQuery);
		navQuery = nullptr;
	}

	if (navMesh)
	{
		dtFreeNavMesh(navMesh);
		navMesh = nullptr;
	}

	built = false;
	debugCells.clear();
	navTiles.clear();
	tileCountX = 0;
	tileCountZ = 0;
}

void NavMeshActor::Update()
{
	if (!buildRequested && !regionBuildRequested) return;

	if (buildDelayFrames > 0)
	{
		--buildDelayFrames;
		return;
	}

	if (buildRequested)
	{
		buildRequested = false;
		regionBuildRequested = false;
		Build();
		return;
	}

	regionBuildRequested = false;
	RebuildRegion();
}

void NavMeshActor::RequestBuild(int delayFrames)
{
	buildRequested = true;
	regionBuildRequested = false;
	buildDelayFrames = std::max(delayFrames, 0);
}

void NavMeshActor::RequestBuildRegion(const Vector3& center, float radius, int delayFrames)
{
	radius = std::max(radius, 0.0f);
	const Vector3 regionRadius(radius, 0.0f, radius);
	const Vector3 regionMin = center - regionRadius;
	const Vector3 regionMax = center + regionRadius;
	if (!regionBuildRequested)
	{
		rebuildRegionMin = regionMin;
		rebuildRegionMax = regionMax;
	}
	else
	{
		rebuildRegionMin.x = std::min(rebuildRegionMin.x, regionMin.x);
		rebuildRegionMin.z = std::min(rebuildRegionMin.z, regionMin.z);
		rebuildRegionMax.x = std::max(rebuildRegionMax.x, regionMax.x);
		rebuildRegionMax.z = std::max(rebuildRegionMax.z, regionMax.z);
	}

	regionBuildRequested = true;
	buildDelayFrames = std::max(delayFrames, 0);
}

void NavMeshActor::SetAgentRadius(float value)
{
	value = std::max(value, 0.0f);
	if (fabsf(agentRadius - value) <= 0.001f) return;

	agentRadius = value;
	RequestBuild();
}

void NavMeshActor::SetResolution(int value)
{
	value = std::clamp(value, 8, 2048);
	if (resolution == value) return;

	resolution = value;
	RequestBuild();
}

void NavMeshActor::AddWalkableArea(const Vector3& center, const Vector3& size)
{
	WalkableArea area;
	area.center = center;
	area.size = Vector3(std::max(fabsf(size.x), 0.1f), std::max(fabsf(size.y), 0.1f),
		std::max(fabsf(size.z), 0.1f));
	walkableAreas.push_back(area);
	RequestBuild();
}

void NavMeshActor::ClearWalkableAreas()
{
	if (walkableAreas.empty()) return;

	walkableAreas.clear();
	RequestBuild();
}

std::string NavMeshActor::SaveSettingsJson() const
{
	json root;
	root["resolution"] = resolution;
	root["agentHeight"] = agentHeight;
	root["agentRadius"] = agentRadius;
	root["agentClimb"] = agentClimb;
	root["agentMaxSlope"] = agentMaxSlope;
	root["nearestPolyExtent"] = nearestPolyExtent;
	root["navMinY"] = navMinY;
	root["navMaxY"] = navMaxY;

	for (const WalkableArea& area : walkableAreas)
	{
		root["walkableAreas"].push_back({{"center", {area.center.x, area.center.y, area.center.z}},
			{"size", {area.size.x, area.size.y, area.size.z}}});
	}
	return root.dump();
}

bool NavMeshActor::LoadSettingsJson(const std::string& text)
{
	if (text.empty())
	{
		RequestBuild();
		return true;
	}

	try
	{
		const json root = json::parse(text);
		resolution = std::clamp(root.value("resolution", resolution), 8, 2048);
		agentHeight = std::max(root.value("agentHeight", agentHeight), 0.1f);
		agentRadius = std::max(root.value("agentRadius", agentRadius), 0.0f);
		agentClimb = std::max(root.value("agentClimb", agentClimb), 0.0f);
		agentMaxSlope = std::clamp(root.value("agentMaxSlope", agentMaxSlope), 0.0f, 89.0f);
		nearestPolyExtent = std::max(root.value("nearestPolyExtent", nearestPolyExtent), 0.1f);
		navMinY = root.value("navMinY", navMinY);
		navMaxY = root.value("navMaxY", navMaxY);
		if (navMinY > navMaxY) std::swap(navMinY, navMaxY);

		walkableAreas.clear();
		for (const json& value : root.value("walkableAreas", json::array()))
		{
			const json center = value.value("center", json::array());
			const json size = value.value("size", json::array());
			if (center.size() < 3 || size.size() < 3) continue;
			AddWalkableArea(
				Vector3(center[0].get<float>(), center[1].get<float>(), center[2].get<float>()),
				Vector3(size[0].get<float>(), size[1].get<float>(), size[2].get<float>()));
		}
		RequestBuild();
		return true;
	}
	catch (const json::exception&)
	{
		statusMessage = (const char*)u8"ナビメッシュ設定の読み込みに失敗しました（JSONが不正です）";
		return false;
	}
}

void NavMeshActor::CollectObstacles(std::vector<ObstacleBounds>& obstacles) const
{
	ActorManager* actorManager = ActorManager::GetActive();
	if (!actorManager) return;

	for (Actor* other : actorManager->GetActors())
	{
		if (!other || other->IsPendingDestroy()) continue;

		NavMeshObstacle* obstacle = other->GetComponent<NavMeshObstacle>();
		if (!obstacle) continue;

		ObstacleBounds bounds;
		if (!obstacle->GetBounds(bounds.center, bounds.size)) continue;

		obstacles.push_back(bounds);
	}
}

bool NavMeshActor::IsBlockedByObstacle(
	const Vector3& center, const std::vector<ObstacleBounds>& obstacles, float cellHalfSize) const
{
	for (const ObstacleBounds& obstacle : obstacles)
	{
		const Vector3 halfSize = obstacle.size * 0.5f;
		const float inflate = cellHalfSize + agentRadius;

		if (center.x < obstacle.center.x - halfSize.x - inflate) continue;
		if (center.x > obstacle.center.x + halfSize.x + inflate) continue;
		if (center.z < obstacle.center.z - halfSize.z - inflate) continue;
		if (center.z > obstacle.center.z + halfSize.z + inflate) continue;

		return true;
	}

	return false;
}

bool NavMeshActor::IsInsideWalkableArea(const Vector3& center, float cellHalfSize) const
{
	if (walkableAreas.empty()) return true;

	for (const WalkableArea& area : walkableAreas)
	{
		const Vector3 halfSize = area.size * 0.5f;
		const float clearance = cellHalfSize + agentRadius;
		if (center.x - clearance < area.center.x - halfSize.x) continue;
		if (center.x + clearance > area.center.x + halfSize.x) continue;
		if (center.y < area.center.y - halfSize.y) continue;
		if (center.y > area.center.y + halfSize.y) continue;
		if (center.z - clearance < area.center.z - halfSize.z) continue;
		if (center.z + clearance > area.center.z + halfSize.z) continue;
		return true;
	}

	return false;
}

bool NavMeshActor::IsSegmentInsideWalkableAreas(const Vector3& start, const Vector3& goal) const
{
	if (walkableAreas.empty()) return true;

	const Vector3 delta = goal - start;
	const float horizontalDistance = Vector2(delta.x, delta.z).Length();
	Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr;
	const float cellSize =
		terrain ? terrain->GetTerrainSize() / static_cast<float>(std::max(resolution, 1)) : 1.0f;
	const float sampleInterval = std::max(cellSize * 0.5f, 0.1f);
	const int sampleCount =
		std::max(static_cast<int>(ceilf(horizontalDistance / sampleInterval)), 1);

	for (int index = 0; index <= sampleCount; ++index)
	{
		const float rate = static_cast<float>(index) / static_cast<float>(sampleCount);
		if (!IsInsideWalkableArea(start + delta * rate, 0.0f)) return false;
	}

	return true;
}

// 動的地形用NavMeshタイル

void NavMeshActor::Build()
{
	Release();

	Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr;
	if (!terrain)
	{
		statusMessage = (const char*)u8"地形がないためナビメッシュを生成できません";
		return;
	}

	const int grid = std::max(resolution, 2);
	const float terrainSize = terrain->GetTerrainSize();
	const float cellSize = terrainSize / static_cast<float>(grid);
	const float halfTerrainSize = terrainSize * 0.5f;
	tileCountX = (grid + CellsPerTile - 1) / CellsPerTile;
	tileCountZ = (grid + CellsPerTile - 1) / CellsPerTile;

	dtNavMeshParams params = {};
	params.orig[0] = -halfTerrainSize;
	params.orig[1] = navMinY;
	params.orig[2] = -halfTerrainSize;
	params.tileWidth = cellSize * CellsPerTile;
	params.tileHeight = cellSize * CellsPerTile;
	params.maxTiles = tileCountX * tileCountZ;
	params.maxPolys = CellsPerTile * CellsPerTile * 2 + 1;

	navMesh = dtAllocNavMesh();
	if (!navMesh)
	{
		statusMessage = (const char*)u8"ナビメッシュのメモリ確保に失敗しました";
		return;
	}

	if (dtStatusFailed(navMesh->init(&params)))
	{
		Release();
		statusMessage = (const char*)u8"ナビメッシュの初期化に失敗しました";
		return;
	}

	navTiles.resize(static_cast<size_t>(tileCountX) * tileCountZ);
	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);
	for (int tileZ = 0; tileZ < tileCountZ; ++tileZ)
	{
		for (int tileX = 0; tileX < tileCountX; ++tileX)
		{
			if (!BuildTile(tileX, tileZ, *terrain, obstacles))
			{
				Release();
				statusMessage = (const char*)u8"ナビメッシュタイルの生成に失敗しました";
				return;
			}
		}
	}

	navQuery = dtAllocNavMeshQuery();
	if (!navQuery)
	{
		Release();
		statusMessage = (const char*)u8"ナビメッシュ検索のメモリ確保に失敗しました";
		return;
	}

	if (dtStatusFailed(navQuery->init(navMesh, 8192)))
	{
		Release();
		statusMessage = (const char*)u8"ナビメッシュ検索の初期化に失敗しました";
		return;
	}

	built = true;
	RefreshDebugCells();
	statusMessage = (const char*)u8"タイル生成完了  タイル=" +
		std::to_string(tileCountX * tileCountZ) +
		(const char*)u8" 障害物=" + std::to_string(obstacles.size());
}

bool NavMeshActor::BuildTile(
	int tileX,
	int tileZ,
	Terrain& terrain,
	const std::vector<ObstacleBounds>& obstacles)
{
	const int grid = std::max(resolution, 2);
	const int minGridX = tileX * CellsPerTile;
	const int minGridZ = tileZ * CellsPerTile;
	const int maxGridX = std::min(minGridX + CellsPerTile, grid);
	const int maxGridZ = std::min(minGridZ + CellsPerTile, grid);
	const int cellCountX = maxGridX - minGridX;
	const int cellCountZ = maxGridZ - minGridZ;
	if (cellCountX <= 0 || cellCountZ <= 0) return true;

	const int tileIndex = tileZ * tileCountX + tileX;
	NavTile& tile = navTiles[tileIndex];
	if (tile.reference)
	{
		unsigned char* oldData = nullptr;
		int oldDataSize = 0;
		navMesh->removeTile(tile.reference, &oldData, &oldDataSize);
		dtFree(oldData);
		tile.reference = 0;
	}
	tile.debugCells.clear();

	constexpr int nvp = 3;
	const float terrainSize = terrain.GetTerrainSize();
	const float cellSize = terrainSize / static_cast<float>(grid);
	const float cellHeight = 0.25f;
	const float halfTerrainSize = terrainSize * 0.5f;
	std::vector<unsigned short> verts(
		static_cast<size_t>(cellCountX + 1) * (cellCountZ + 1) * 3);
	std::vector<Vector3> worldVertices(
		static_cast<size_t>(cellCountX + 1) * (cellCountZ + 1));
	std::vector<unsigned short> polys;
	std::vector<unsigned short> flags;
	std::vector<unsigned char> areas;

	for (int localZ = 0; localZ <= cellCountZ; ++localZ)
	{
		for (int localX = 0; localX <= cellCountX; ++localX)
		{
			const int gridX = minGridX + localX;
			const int gridZ = minGridZ + localZ;
			const float u = static_cast<float>(gridX) / static_cast<float>(grid);
			const float v = static_cast<float>(gridZ) / static_cast<float>(grid);
			const int vertexIndex = localZ * (cellCountX + 1) + localX;
			Vector3& position = worldVertices[vertexIndex];
			position = Vector3(
				(u - 0.5f) * terrainSize,
				terrain.GetHeightByUV(u, v),
				(v - 0.5f) * terrainSize);

			verts[static_cast<size_t>(vertexIndex) * 3] =
				static_cast<unsigned short>(localX);
			verts[static_cast<size_t>(vertexIndex) * 3 + 1] =
				static_cast<unsigned short>(std::clamp(
					static_cast<int>((position.y - navMinY) / cellHeight), 0, 0xffff));
			verts[static_cast<size_t>(vertexIndex) * 3 + 2] =
				static_cast<unsigned short>(localZ);
		}
	}

	const float minWalkableNormalY = cosf(RAD(agentMaxSlope));
	for (int localZ = 0; localZ < cellCountZ; ++localZ)
	{
		for (int localX = 0; localX < cellCountX; ++localX)
		{
			const int gridX = minGridX + localX;
			const int gridZ = minGridZ + localZ;
			const float u = (static_cast<float>(gridX) + 0.5f) / static_cast<float>(grid);
			const float v = (static_cast<float>(gridZ) + 0.5f) / static_cast<float>(grid);
			const Vector3 center(
				(u - 0.5f) * terrainSize,
				terrain.GetHeightByUV(u, v),
				(v - 0.5f) * terrainSize);
			if (!IsInsideWalkableArea(center, cellSize * 0.5f)) continue;

			const unsigned short i0 =
				static_cast<unsigned short>(localZ * (cellCountX + 1) + localX);
			const unsigned short i1 = i0 + 1;
			const unsigned short i2 = i0 + static_cast<unsigned short>(cellCountX + 1);
			const unsigned short i3 = i2 + 1;
			const Vector3& p0 = worldVertices[i0];
			const Vector3& p1 = worldVertices[i1];
			const Vector3& p2 = worldVertices[i2];
			const Vector3& p3 = worldVertices[i3];

			if (IsBlockedByObstacle(center, obstacles, cellSize * 0.5f))
			{
				DebugCell firstCell;
				firstCell.corners[0] = p0;
				firstCell.corners[1] = p2;
				firstCell.corners[2] = p1;
				tile.debugCells.push_back(firstCell);

				DebugCell secondCell;
				secondCell.corners[0] = p1;
				secondCell.corners[1] = p2;
				secondCell.corners[2] = p3;
				tile.debugCells.push_back(secondCell);
				continue;
			}

			Vector3 firstNormal = (p2 - p0).Cross(p1 - p0);
			Vector3 secondNormal = (p2 - p1).Cross(p3 - p1);
			const bool hasFirstNormal = firstNormal.LengthSquared() > eps;
			const bool hasSecondNormal = secondNormal.LengthSquared() > eps;
			if (hasFirstNormal) firstNormal.Normalize();
			if (hasSecondNormal) secondNormal.Normalize();
			const bool firstWalkable =
				hasFirstNormal && fabsf(firstNormal.y) >= minWalkableNormalY;
			const bool secondWalkable =
				hasSecondNormal && fabsf(secondNormal.y) >= minWalkableNormalY;

			DebugCell firstCell;
			firstCell.corners[0] = p0;
			firstCell.corners[1] = p2;
			firstCell.corners[2] = p1;
			firstCell.walkable = firstWalkable;
			tile.debugCells.push_back(firstCell);

			DebugCell secondCell;
			secondCell.corners[0] = p1;
			secondCell.corners[1] = p2;
			secondCell.corners[2] = p3;
			secondCell.walkable = secondWalkable;
			tile.debugCells.push_back(secondCell);

			if (firstWalkable)
			{
				const size_t polygonOffset = polys.size();
				polys.resize(polygonOffset + nvp * 2, 0xffff);
				polys[polygonOffset] = i0;
				polys[polygonOffset + 1] = i2;
				polys[polygonOffset + 2] = i1;
				flags.push_back(1);
				areas.push_back(0);
			}
			if (secondWalkable)
			{
				const size_t polygonOffset = polys.size();
				polys.resize(polygonOffset + nvp * 2, 0xffff);
				polys[polygonOffset] = i1;
				polys[polygonOffset + 1] = i2;
				polys[polygonOffset + 2] = i3;
				flags.push_back(1);
				areas.push_back(0);
			}
		}
	}

	if (flags.empty()) return true;

	struct EdgeRef
	{
		unsigned short polygon = 0xffff;
		unsigned short edge = 0xffff;
	};
	std::map<std::pair<unsigned short, unsigned short>, EdgeRef> edgeRefs;
	for (int polygonIndex = 0; polygonIndex < static_cast<int>(flags.size()); ++polygonIndex)
	{
		unsigned short* polygon = &polys[static_cast<size_t>(polygonIndex) * nvp * 2];
		for (int edgeIndex = 0; edgeIndex < nvp; ++edgeIndex)
		{
			const unsigned short a = polygon[edgeIndex];
			const unsigned short b = polygon[(edgeIndex + 1) % nvp];
			const std::pair<unsigned short, unsigned short> key(std::min(a, b), std::max(a, b));
			auto [it, inserted] = edgeRefs.emplace(
				key, EdgeRef{static_cast<unsigned short>(polygonIndex),
							 static_cast<unsigned short>(edgeIndex)});
			if (inserted) continue;

			unsigned short* other =
				&polys[static_cast<size_t>(it->second.polygon) * nvp * 2];
			polygon[nvp + edgeIndex] = it->second.polygon;
			other[nvp + it->second.edge] = static_cast<unsigned short>(polygonIndex);
		}
	}

	for (int polygonIndex = 0; polygonIndex < static_cast<int>(flags.size()); ++polygonIndex)
	{
		unsigned short* polygon = &polys[static_cast<size_t>(polygonIndex) * nvp * 2];
		for (int edgeIndex = 0; edgeIndex < nvp; ++edgeIndex)
		{
			if (polygon[nvp + edgeIndex] != 0xffff) continue;
			const unsigned short a = polygon[edgeIndex];
			const unsigned short b = polygon[(edgeIndex + 1) % nvp];
			const int ax = verts[static_cast<size_t>(a) * 3];
			const int az = verts[static_cast<size_t>(a) * 3 + 2];
			const int bx = verts[static_cast<size_t>(b) * 3];
			const int bz = verts[static_cast<size_t>(b) * 3 + 2];

			if (ax == 0 && bx == 0 && tileX > 0)
				polygon[nvp + edgeIndex] = 0x8000 | 0;
			else if (az == cellCountZ && bz == cellCountZ && tileZ + 1 < tileCountZ)
				polygon[nvp + edgeIndex] = 0x8000 | 1;
			else if (ax == cellCountX && bx == cellCountX && tileX + 1 < tileCountX)
				polygon[nvp + edgeIndex] = 0x8000 | 2;
			else if (az == 0 && bz == 0 && tileZ > 0)
				polygon[nvp + edgeIndex] = 0x8000 | 3;
		}
	}

	dtNavMeshCreateParams params = {};
	params.verts = verts.data();
	params.vertCount = static_cast<int>(verts.size() / 3);
	params.polys = polys.data();
	params.polyFlags = flags.data();
	params.polyAreas = areas.data();
	params.polyCount = static_cast<int>(flags.size());
	params.nvp = nvp;
	params.tileX = tileX;
	params.tileY = tileZ;
	params.bmin[0] = minGridX * cellSize - halfTerrainSize;
	params.bmin[1] = navMinY;
	params.bmin[2] = minGridZ * cellSize - halfTerrainSize;
	params.bmax[0] = maxGridX * cellSize - halfTerrainSize;
	params.bmax[1] = navMaxY;
	params.bmax[2] = maxGridZ * cellSize - halfTerrainSize;
	params.walkableHeight = agentHeight;
	params.walkableRadius = agentRadius;
	params.walkableClimb = agentClimb;
	params.cs = cellSize;
	params.ch = cellHeight;
	params.buildBvTree = true;

	unsigned char* data = nullptr;
	int dataSize = 0;
	if (!dtCreateNavMeshData(&params, &data, &dataSize)) return false;
	if (dtStatusFailed(navMesh->addTile(
		data, dataSize, DT_TILE_FREE_DATA, 0, &tile.reference)))
	{
		dtFree(data);
		return false;
	}
	return true;
}

void NavMeshActor::RebuildRegion()
{
	if (!built || !navMesh)
	{
		Build();
		return;
	}

	Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr;
	if (!terrain)
	{
		statusMessage = (const char*)u8"地形がないためナビメッシュを再生成できません";
		return;
	}

	const int grid = std::max(resolution, 2);
	const float terrainSize = terrain->GetTerrainSize();
	const float halfTerrainSize = terrainSize * 0.5f;
	const float tileSize = terrainSize / static_cast<float>(grid) * CellsPerTile;
	const int minTileX = std::clamp(static_cast<int>(floorf(
		(rebuildRegionMin.x + halfTerrainSize) / tileSize)), 0, tileCountX - 1);
	const int minTileZ = std::clamp(static_cast<int>(floorf(
		(rebuildRegionMin.z + halfTerrainSize) / tileSize)), 0, tileCountZ - 1);
	const int maxTileX = std::clamp(static_cast<int>(floorf(
		(rebuildRegionMax.x + halfTerrainSize) / tileSize)), 0, tileCountX - 1);
	const int maxTileZ = std::clamp(static_cast<int>(floorf(
		(rebuildRegionMax.z + halfTerrainSize) / tileSize)), 0, tileCountZ - 1);

	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);
	int rebuiltTileCount = 0;
	for (int tileZ = minTileZ; tileZ <= maxTileZ; ++tileZ)
	{
		for (int tileX = minTileX; tileX <= maxTileX; ++tileX)
		{
			if (!BuildTile(tileX, tileZ, *terrain, obstacles))
			{
				statusMessage = (const char*)u8"NavMeshタイルの局所再生成に失敗しました";
				return;
			}
			++rebuiltTileCount;
		}
	}

	RefreshDebugCells();
	statusMessage = (const char*)u8"局所再生成完了  タイル=" +
		std::to_string(rebuiltTileCount);
}

void NavMeshActor::RefreshDebugCells()
{
	debugCells.clear();
	for (const NavTile& tile : navTiles)
	{
		debugCells.insert(debugCells.end(), tile.debugCells.begin(), tile.debugCells.end());
	}
}

bool NavMeshActor::FindNextPoint(
	const Vector3& start, const Vector3& goal, Vector3& nextPoint, Vector3* reachableGoal) const
{
	if (!built || !navQuery) return false;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	filter.setExcludeFlags(0);

	const float horizontalExtent = std::max(nearestPolyExtent, agentRadius * 2.0f);
	const float halfExtents[3] = {horizontalExtent, 20.0f, horizontalExtent};
	const float startPos[3] = {start.x, start.y, start.z};
	const float goalPos[3] = {goal.x, goal.y, goal.z};
	float nearestStart[3] = {};
	float nearestGoal[3] = {};
	dtPolyRef startRef = 0;
	dtPolyRef goalRef = 0;

	if (dtStatusFailed(
			navQuery->findNearestPoly(startPos, halfExtents, &filter, &startRef, nearestStart)) ||
		!startRef)
		return false;

	if (dtStatusFailed(
			navQuery->findNearestPoly(goalPos, halfExtents, &filter, &goalRef, nearestGoal)) ||
		!goalRef)
		return false;

	constexpr int maxPathPolys = 2048;
	dtPolyRef path[maxPathPolys] = {};
	int pathCount = 0;
	if (dtStatusFailed(navQuery->findPath(
			startRef, goalRef, nearestStart, nearestGoal, &filter, path, &pathCount, maxPathPolys)))
		return false;

	if (pathCount <= 0) return false;

	float pathGoal[3] = {nearestGoal[0], nearestGoal[1], nearestGoal[2]};
	if (path[pathCount - 1] != goalRef)
	{
		bool positionOverPoly = false;
		if (dtStatusFailed(navQuery->closestPointOnPoly(
				path[pathCount - 1], goalPos, pathGoal, &positionOverPoly)))
		{
			return false;
		}
	}

	if (reachableGoal)
	{
		*reachableGoal = Vector3(pathGoal[0], pathGoal[1], pathGoal[2]);
	}

	constexpr int maxStraightPoints = 256;
	float straightPath[maxStraightPoints * 3] = {};
	unsigned char straightFlags[maxStraightPoints] = {};
	dtPolyRef straightRefs[maxStraightPoints] = {};
	int straightCount = 0;
	if (dtStatusFailed(navQuery->findStraightPath(nearestStart, pathGoal, path, pathCount,
			straightPath, straightFlags, straightRefs, &straightCount, maxStraightPoints)))
	{
		return false;
	}

	if (straightCount <= 0) return false;

	int pointIndex = straightCount - 1;
	const float minNextPointDistance = std::max(agentRadius * 0.1f, 0.05f);
	const float minNextPointDistanceSq = minNextPointDistance * minNextPointDistance;
	for (int index = 1; index < straightCount; ++index)
	{
		Vector3 delta(
			straightPath[index * 3 + 0] - start.x, 0.0f, straightPath[index * 3 + 2] - start.z);
		if (delta.LengthSquared() <= minNextPointDistanceSq) continue;

		pointIndex = index;
		break;
	}

	nextPoint = Vector3(straightPath[pointIndex * 3 + 0], straightPath[pointIndex * 3 + 1],
		straightPath[pointIndex * 3 + 2]);
	return true;
}

bool NavMeshActor::FindNearestPoint(const Vector3& position, Vector3& nearestPoint) const
{
	if (!built || !navQuery) return false;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	filter.setExcludeFlags(0);

	const float horizontalExtent = std::max(nearestPolyExtent, agentRadius * 2.0f);
	const float halfExtents[3] = {horizontalExtent, 20.0f, horizontalExtent};
	const float queryPosition[3] = {position.x, position.y, position.z};
	float nearestPosition[3] = {};
	dtPolyRef nearestRef = 0;
	if (dtStatusFailed(navQuery->findNearestPoly(
			queryPosition, halfExtents, &filter, &nearestRef, nearestPosition)) ||
		!nearestRef)
	{
		return false;
	}

	nearestPoint = {nearestPosition[0], nearestPosition[1], nearestPosition[2]};
	return true;
}

bool NavMeshActor::FindRecoveryPoint(
	const Vector3& position, float safeDistance, Vector3& recoveryPoint) const
{
	if (!built || !navQuery) return false;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	filter.setExcludeFlags(0);

	const float horizontalExtent = std::max(nearestPolyExtent, agentRadius * 2.0f);
	const float halfExtents[3] = {horizontalExtent, 20.0f, horizontalExtent};
	const float queryPosition[3] = {position.x, position.y, position.z};
	float nearestPosition[3] = {};
	dtPolyRef nearestRef = 0;
	if (dtStatusFailed(navQuery->findNearestPoly(
			queryPosition, halfExtents, &filter, &nearestRef, nearestPosition)) ||
		!nearestRef)
	{
		return false;
	}

	recoveryPoint = {nearestPosition[0], nearestPosition[1], nearestPosition[2]};
	safeDistance = std::max(safeDistance, 0.0f);
	if (safeDistance <= eps) return true;

	float wallDistance = safeDistance;
	float wallPosition[3] = {};
	float wallNormal[3] = {};
	if (dtStatusFailed(navQuery->findDistanceToWall(nearestRef, nearestPosition, safeDistance,
			&filter, &wallDistance, wallPosition, wallNormal)) ||
		wallDistance >= safeDistance)
	{
		return true;
	}

	Vector3 inward(wallNormal[0], 0.0f, wallNormal[2]);
	if (inward.LengthSquared() <= eps) return true;
	inward.Normalize();
	recoveryPoint += inward * (safeDistance - wallDistance);
	return true;
}

bool NavMeshActor::FindRandomPoint(
	const Vector3& center, float minDistance, float maxDistance, Vector3& randomPoint) const
{
	if (!built || !navQuery) return false;

	minDistance = std::max(minDistance, 0.0f);
	maxDistance = std::max(maxDistance, 0.0f);
	if (minDistance > maxDistance) std::swap(minDistance, maxDistance);
	if (maxDistance <= eps) return false;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	filter.setExcludeFlags(0);

	const float horizontalExtent = std::max(nearestPolyExtent, agentRadius * 2.0f);
	const float halfExtents[3] = {horizontalExtent, 20.0f, horizontalExtent};
	const float centerPos[3] = {center.x, center.y, center.z};
	float nearestCenter[3] = {};
	dtPolyRef centerRef = 0;
	if (dtStatusFailed(navQuery->findNearestPoly(
			centerPos, halfExtents, &filter, &centerRef, nearestCenter)) ||
		!centerRef)
	{
		return false;
	}

	constexpr int maxAttempts = 64;
	const float minDistanceSq = minDistance * minDistance;
	const float maxDistanceSq = maxDistance * maxDistance;
	for (int attempt = 0; attempt < maxAttempts; ++attempt)
	{
		const float angle = Random::Range(-DirectX::XM_PI, DirectX::XM_PI);
		const float distance = sqrtf(Random::Range(minDistanceSq, maxDistanceSq));
		const float candidate[3] = {
			center.x + sinf(angle) * distance, center.y, center.z + cosf(angle) * distance};

		dtPolyRef goalRef = 0;
		float nearestGoal[3] = {};
		if (dtStatusFailed(navQuery->findNearestPoly(
				candidate, halfExtents, &filter, &goalRef, nearestGoal)) ||
			!goalRef)
		{
			continue;
		}

		const Vector3 offset(nearestGoal[0] - center.x, 0.0f, nearestGoal[2] - center.z);
		const float distanceSq = offset.LengthSquared();
		if (distanceSq < minDistanceSq || distanceSq > maxDistanceSq) continue;

		dtPolyRef path[64] = {};
		int pathCount = 0;
		if (dtStatusFailed(navQuery->findPath(centerRef, goalRef, nearestCenter, nearestGoal,
				&filter, path, &pathCount, _countof(path))) ||
			pathCount <= 0 || path[pathCount - 1] != goalRef)
		{
			continue;
		}

		randomPoint = Vector3(nearestGoal[0], nearestGoal[1], nearestGoal[2]);
		return true;
	}

	return false;
}

bool NavMeshActor::IsDirectPathBlocked(const Vector3& start, const Vector3& goal) const
{
	if (!IsSegmentInsideWalkableAreas(start, goal)) return true;

	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);

	const Vector3 delta = goal - start;
	for (const ObstacleBounds& obstacle : obstacles)
	{
		const Vector3 halfSize = obstacle.size * 0.5f + Vector3(agentRadius, 0.0f, agentRadius);
		const float minX = obstacle.center.x - halfSize.x;
		const float maxX = obstacle.center.x + halfSize.x;
		const float minZ = obstacle.center.z - halfSize.z;
		const float maxZ = obstacle.center.z + halfSize.z;

		float enter = 0.0f;
		float exit = 1.0f;
		auto clipAxis = [&enter, &exit](
							float origin, float direction, float minValue, float maxValue) {
			if (fabsf(direction) <= eps) return origin >= minValue && origin <= maxValue;

			float first = (minValue - origin) / direction;
			float second = (maxValue - origin) / direction;
			if (first > second) std::swap(first, second);
			enter = std::max(enter, first);
			exit = std::min(exit, second);
			return enter <= exit;
		};

		if (clipAxis(start.x, delta.x, minX, maxX) && clipAxis(start.z, delta.z, minZ, maxZ))
		{
			return true;
		}
	}

	return false;
}

bool NavMeshActor::FindObstacleDetourPoint(
	const Vector3& start, const Vector3& goal, Vector3& nextPoint) const
{
	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);

	auto intersects = [](const Vector3& from, const Vector3& to, const ObstacleBounds& obstacle) {
		const Vector3 halfSize = obstacle.size * 0.5f;
		const Vector3 delta = to - from;
		float enter = 0.0f;
		float exit = 1.0f;
		auto clipAxis = [&enter, &exit](
							float origin, float direction, float minValue, float maxValue) {
			if (fabsf(direction) <= eps) return origin >= minValue && origin <= maxValue;
			float first = (minValue - origin) / direction;
			float second = (maxValue - origin) / direction;
			if (first > second) std::swap(first, second);
			enter = std::max(enter, first);
			exit = std::min(exit, second);
			return enter <= exit;
		};

		return clipAxis(from.x, delta.x, obstacle.center.x - halfSize.x,
				   obstacle.center.x + halfSize.x) &&
			   clipAxis(
				   from.z, delta.z, obstacle.center.z - halfSize.z, obstacle.center.z + halfSize.z);
	};

	float bestDistance = FLT_MAX;
	bool found = false;
	const float clearance = std::max(agentRadius, 0.1f);
	for (const ObstacleBounds& obstacle : obstacles)
	{
		if (!intersects(start, goal, obstacle)) continue;
		const Vector3 halfSize = obstacle.size * 0.5f;
		const Vector3 candidates[] = {Vector3(obstacle.center.x - halfSize.x - clearance, start.y,
										  obstacle.center.z - halfSize.z - clearance),
			Vector3(obstacle.center.x + halfSize.x + clearance, start.y,
				obstacle.center.z - halfSize.z - clearance),
			Vector3(obstacle.center.x - halfSize.x - clearance, start.y,
				obstacle.center.z + halfSize.z + clearance),
			Vector3(obstacle.center.x + halfSize.x + clearance, start.y,
				obstacle.center.z + halfSize.z + clearance)};

		for (const Vector3& candidate : candidates)
		{
			if (!IsInsideWalkableArea(candidate, clearance)) continue;
			if (!IsSegmentInsideWalkableAreas(start, candidate)) continue;
			if (!IsSegmentInsideWalkableAreas(candidate, goal)) continue;

			bool blocked = false;
			for (const ObstacleBounds& other : obstacles)
			{
				if (!intersects(start, candidate, other)) continue;
				blocked = true;
				break;
			}
			if (blocked) continue;

			const float distance =
				Vector3::Distance(start, candidate) + Vector3::Distance(candidate, goal);
			if (distance >= bestDistance) continue;
			bestDistance = distance;
			nextPoint = candidate;
			found = true;
		}
	}

	return found;
}

void NavMeshActor::Render(const RenderContext& rc)
{
	if (!rc.renderSettings.showDebug || !showNavMeshDebug || !rc.renderSettings.showNavMeshDebug)
		return;

	PrimitiveRenderer* renderer = Game::Graphics::Instance().GetPrimitiveRenderer();
	if (!renderer) return;

	const Color walkableColor(0.0f, 0.75f, 1.0f, 0.30f);
	const Color blockedColor(1.0f, 0.15f, 0.05f, 0.65f);

	for (int index = 0; index < static_cast<int>(debugCells.size()); ++index)
	{
		const DebugCell& cell = debugCells[index];
		if (cell.walkable && !showWalkableCells) continue;
		if (!cell.walkable && !showBlockedCells) continue;
		const Color color = cell.walkable ? walkableColor : blockedColor;
		if (cell.walkable)
		{
			renderer->DrawTriangle(cell.corners[0], cell.corners[1], cell.corners[2], color);
			continue;
		}
		renderer->DrawLine(cell.corners[0], cell.corners[1], color, color);
		renderer->DrawLine(cell.corners[1], cell.corners[2], color, color);
		renderer->DrawLine(cell.corners[2], cell.corners[0], color, color);
	}

	if (!showObstacleBounds && !showWalkableAreaBounds) return;

	auto drawBounds = [renderer](const Vector3& center, const Vector3& size, const Color& color) {
		const Vector3 half = size * 0.5f;
		const Vector3 corners[] = {center + Vector3(-half.x, -half.y, -half.z),
			center + Vector3(half.x, -half.y, -half.z), center + Vector3(half.x, -half.y, half.z),
			center + Vector3(-half.x, -half.y, half.z), center + Vector3(-half.x, half.y, -half.z),
			center + Vector3(half.x, half.y, -half.z), center + Vector3(half.x, half.y, half.z),
			center + Vector3(-half.x, half.y, half.z)};
		constexpr int edges[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4},
			{0, 4}, {1, 5}, {2, 6}, {3, 7}};
		for (const auto& edge : edges)
			renderer->DrawLine(corners[edge[0]], corners[edge[1]], color, color);
	};

	if (showObstacleBounds)
	{
		std::vector<ObstacleBounds> obstacles;
		CollectObstacles(obstacles);
		const Color obstacleColor(1.0f, 1.0f, 1.0f, 1.0f);
		for (const ObstacleBounds& obstacle : obstacles)
			drawBounds(obstacle.center, obstacle.size, obstacleColor);
	}

	if (showWalkableAreaBounds)
	{
		const Color walkableAreaColor(0.2f, 1.0f, 0.25f, 1.0f);
		for (const WalkableArea& area : walkableAreas)
			drawBounds(area.center, area.size, walkableAreaColor);
	}
}

void NavMeshActor::DrawGUI()
{
	// ナビメッシュのデバッグ表示
	ImGui::Checkbox((const char*)u8"ナビメッシュを表示", &showNavMeshDebug);
	if (showNavMeshDebug)
	{
		ImGui::Checkbox((const char*)u8"実範囲（面）", &showWalkableCells);
		ImGui::Checkbox((const char*)u8"移動不可セル（線）", &showBlockedCells);
		ImGui::Checkbox((const char*)u8"障害物の境界（線）", &showObstacleBounds);
		ImGui::Checkbox((const char*)u8"入力範囲（線）", &showWalkableAreaBounds);
		ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), (const char*)u8"水色：生成された実範囲");
		ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.05f, 1.0f), (const char*)u8"赤：移動不可セル");
		ImGui::TextUnformatted((const char*)u8"白：障害物の境界");
		ImGui::TextColored(
			ImVec4(0.2f, 1.0f, 0.25f, 1.0f), (const char*)u8"緑：入力した歩行可能範囲");
	}

	// 生成時のエージェント設定
	int editedResolution = resolution;
	if (ImGui::DragInt((const char*)u8"解像度", &editedResolution, 1.0f, 8, 2048))
		SetResolution(editedResolution);
	ImGui::DragFloat((const char*)u8"エージェントの高さ", &agentHeight, 0.1f, 0.1f, 10.0f);
	ImGui::DragFloat((const char*)u8"エージェントの半径", &agentRadius, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat((const char*)u8"登れる段差", &agentClimb, 0.1f, 0.0f, 10.0f);
	float editedMaxSlope = agentMaxSlope;
	if (ImGui::DragFloat((const char*)u8"登れる最大傾斜", &editedMaxSlope, 0.5f, 0.0f, 89.0f))
	{
		agentMaxSlope = std::clamp(editedMaxSlope, 0.0f, 89.0f);
		RequestBuild();
	}
	ImGui::DragFloat(
		(const char*)u8"最近傍ポリゴン検索範囲", &nearestPolyExtent, 0.1f, 0.1f, 50.0f);

	// ナビメッシュを生成する入力範囲
	if (ImGui::TreeNode((const char*)u8"歩行可能な入力範囲"))
	{
		if (walkableAreas.empty())
			ImGui::TextUnformatted((const char*)u8"入力範囲なし：地形全体を対象にします");

		int removeIndex = -1;
		for (int index = 0; index < static_cast<int>(walkableAreas.size()); ++index)
		{
			ImGui::PushID(index);
			WalkableArea& area = walkableAreas[index];
			const std::string label = (const char*)u8"範囲 " + std::to_string(index + 1);
			if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
			{
				bool changed = ImGui::DragFloat3((const char*)u8"中心", &area.center.x, 0.25f);
				changed |=
					ImGui::DragFloat3((const char*)u8"大きさ", &area.size.x, 0.25f, 0.1f, 10000.0f);
				if (changed)
				{
					area.size.x = std::max(fabsf(area.size.x), 0.1f);
					area.size.y = std::max(fabsf(area.size.y), 0.1f);
					area.size.z = std::max(fabsf(area.size.z), 0.1f);
					RequestBuild();
				}

				if (ImGui::Button((const char*)u8"削除")) removeIndex = index;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		if (removeIndex >= 0)
		{
			walkableAreas.erase(walkableAreas.begin() + removeIndex);
			RequestBuild();
		}

		if (ImGui::Button((const char*)u8"入力範囲を追加"))
		{
			Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr;
			const float size = terrain ? terrain->GetTerrainSize() : 10.0f;
			AddWalkableArea(Vector3::Zero, Vector3(size, navMaxY - navMinY, size));
		}
		ImGui::TreePop();
	}

	// 生成状態と手動再生成
	ImGui::Text("%s", statusMessage.c_str());

	if (ImGui::Button((const char*)u8"ナビメッシュを再生成")) buildRequested = true;
}
