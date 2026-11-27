#include <imgui.h>
#include <imgui_stdlib.h>
#include <ProfileKit/ProfileKit.h>
#include "Physics/Navigation/NavMeshActor.h"

#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Rendering/Core/Graphics.h"
#include "Physics/Navigation/NavMeshObstacle.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Rendering/Renderer/PrimitiveRenderer.h"
#include "Rendering/Core/RenderContext.h"
#include "Rendering/Component/Terrain.h"
#include "Core/Foundation/Json.h"

#include "DetourAlloc.h"
#include "DetourNavMeshBuilder.h"
#include "DetourStatus.h"

#include <cmath>
#include <cstdlib>
#include <memory>
#include "Recast.h"

NavMeshActor* NavMeshActor::active = nullptr;

NavMeshActor::NavMeshActor(Object* owner) : Component(owner)
{
}

void NavMeshActor::OnAwake()
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
	PROFILE_FUNCTION();
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

std::string NavMeshActor::SaveSettingsJson() const
{
	json root;
	root["generationVersion"] = 3;
	root["simplifyFlatAreas"] = simplifyFlatAreas;
	root["simplifyHeightError"] = simplifyHeightError;
	root["simplifyMaxCells"] = simplifyMaxCells;
	root["navMinY"] = navMinY;
	root["navMaxY"] = navMaxY;

	return root.dump();
}

bool NavMeshActor::LoadSettingsJson(const std::string& text)
{
	try
	{
		const json root = json::parse(text);
		simplifyFlatAreas = root.at("simplifyFlatAreas").get<bool>();
		simplifyHeightError = std::clamp(root.at("simplifyHeightError").get<float>(), 0.0f, 1.0f);
		simplifyMaxCells = std::clamp(root.at("simplifyMaxCells").get<int>(), 1, CellsPerTile);
		navMinY = root.at("navMinY").get<float>();
		navMaxY = root.at("navMaxY").get<float>();
		if (navMinY > navMaxY) std::swap(navMinY, navMaxY);

		RequestBuild();
		return true;
	}
	catch (const json::exception&)
	{
		statusMessage = "ナビメッシュ設定の読み込みに失敗しました（JSONが不正です）";
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

		// VMDLでは、指定されたコライダーを個別の障害物として扱う。
		bool hasVmdlObstacles = false;
		for (auto* collider : other->GetComponents<VMDLColliderComponent>())
		{
			if (!collider->IsNavMeshObstacle()) continue;
			hasVmdlObstacles = true;
			ObstacleBounds bounds;
			if (collider->GetBounds(bounds.center, bounds.size)) obstacles.push_back(bounds);
		}
		if (hasVmdlObstacles) continue;

		ObstacleBounds bounds;
		if (!obstacle->GetBounds(bounds.center, bounds.size)) continue;

		obstacles.push_back(bounds);
	}
}

// 動的地形用NavMeshタイル

void NavMeshActor::Build()
{
	PROFILE_FUNCTION();
	Release();

	Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr;
	if (!terrain)
	{
		statusMessage = "地形がないためナビメッシュを生成できません";
		return;
	}

	const int grid = std::max(terrain->GetHeightMapWidth(), 2);
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
		statusMessage = "ナビメッシュのメモリ確保に失敗しました";
		return;
	}

	if (dtStatusFailed(navMesh->init(&params)))
	{
		Release();
		statusMessage = "ナビメッシュの初期化に失敗しました";
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
				statusMessage = "ナビメッシュタイルの生成に失敗しました";
				return;
			}
		}
	}

	navQuery = dtAllocNavMeshQuery();
	if (!navQuery)
	{
		Release();
		statusMessage = "ナビメッシュ検索のメモリ確保に失敗しました";
		return;
	}

	if (dtStatusFailed(navQuery->init(navMesh, 8192)))
	{
		Release();
		statusMessage = "ナビメッシュ検索の初期化に失敗しました";
		return;
	}

	built = true;
	RefreshDebugCells();
	statusMessage = "タイル生成完了  タイル=" +
		std::to_string(tileCountX * tileCountZ);
}

bool NavMeshActor::BuildTile(int tileX, int tileZ, Terrain& terrain,
	const std::vector<ObstacleBounds>& obstacles)
{
	PROFILE_FUNCTION();
	const int grid = terrain.GetHeightMapWidth();
	const float size = terrain.GetTerrainSize();
	const float halfSize = size * 0.5f;

	// 隣接タイルの領域も含めて生成し、Recastに境界の接続を任せる。
	rcConfig config{};
	config.cs = size / grid;
	config.ch = std::max(0.05f, (navMaxY - navMinY) / (RC_SPAN_MAX_HEIGHT - 1));
	config.walkableSlopeAngle = agentMaxSlope;
	config.walkableHeight = std::max(3, static_cast<int>(std::ceil(agentHeight / config.ch)));
	config.walkableClimb = static_cast<int>(std::floor(agentMaxClimb / config.ch));
	config.walkableRadius = static_cast<int>(std::ceil(agentRadius / config.cs));
	config.tileSize = CellsPerTile;
	config.borderSize = config.walkableRadius + 3;
	config.width = config.tileSize + config.borderSize * 2;
	config.height = config.width;
	config.maxEdgeLen = simplifyFlatAreas ? simplifyMaxCells : 1;
	config.maxSimplificationError = simplifyFlatAreas ? contourMaxError : 0.0f;
	config.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
	config.detailSampleDist = config.cs;
	config.detailSampleMaxError = simplifyFlatAreas ? simplifyHeightError : 0.0f;
	config.bmin[0] = (tileX * CellsPerTile - config.borderSize) * config.cs - halfSize;
	config.bmin[1] = navMinY;
	config.bmin[2] = (tileZ * CellsPerTile - config.borderSize) * config.cs - halfSize;
	config.bmax[0] = config.bmin[0] + config.width * config.cs;
	config.bmax[1] = navMaxY;
	config.bmax[2] = config.bmin[2] + config.height * config.cs;

	// 高さと範囲マスクをRecastの入力三角形へ変換する。
	const int minX = std::max(tileX * CellsPerTile - config.borderSize, 0);
	const int minZ = std::max(tileZ * CellsPerTile - config.borderSize, 0);
	const int maxX = std::min((tileX + 1) * CellsPerTile + config.borderSize, grid);
	const int maxZ = std::min((tileZ + 1) * CellsPerTile + config.borderSize, grid);
	const int columns = maxX - minX + 1;
	std::vector<float> vertices;
	std::vector<int> triangles;
	std::vector<unsigned char> masks;
	vertices.reserve(static_cast<size_t>(columns) * (maxZ - minZ + 1) * 3);
	for (int z = minZ; z <= maxZ; ++z)
	{
		for (int x = minX; x <= maxX; ++x)
		{
			const float u = static_cast<float>(x) / grid;
			const float v = static_cast<float>(z) / grid;
			vertices.push_back((u - 0.5f) * size);
			vertices.push_back(terrain.GetSurfaceHeightByUV(u, v, true));
			vertices.push_back((v - 0.5f) * size);
		}
	}
	for (int z = minZ; z < maxZ; ++z)
	{
		for (int x = minX; x < maxX; ++x)
		{
			const int i = (z - minZ) * columns + x - minX;
			triangles.insert(triangles.end(), {i, i + columns, i + 1,
				i + 1, i + columns, i + columns + 1});
			const bool painted = terrain.GetNavMeshMaskByUV(
				(x + 0.5f) / grid, (z + 0.5f) / grid) >= 0.5f;
			masks.push_back(painted);
			masks.push_back(painted);
		}
	}

	rcContext context;
	std::unique_ptr<rcHeightfield, decltype(&rcFreeHeightField)> field(rcAllocHeightfield(), rcFreeHeightField);
	std::unique_ptr<rcCompactHeightfield, decltype(&rcFreeCompactHeightfield)> compact(
		rcAllocCompactHeightfield(), rcFreeCompactHeightfield);
	std::unique_ptr<rcContourSet, decltype(&rcFreeContourSet)> contours(rcAllocContourSet(), rcFreeContourSet);
	std::unique_ptr<rcPolyMesh, decltype(&rcFreePolyMesh)> mesh(rcAllocPolyMesh(), rcFreePolyMesh);
	std::unique_ptr<rcPolyMeshDetail, decltype(&rcFreePolyMeshDetail)> detail(rcAllocPolyMeshDetail(), rcFreePolyMeshDetail);
	if (!field || !compact || !contours || !mesh || !detail) return false;

	const int triangleCount = static_cast<int>(triangles.size() / 3);
	const int vertexCount = static_cast<int>(vertices.size() / 3);
	std::vector<unsigned char> areas(triangleCount, RC_NULL_AREA);
	rcMarkWalkableTriangles(&context, config.walkableSlopeAngle,
		vertices.data(), vertexCount, triangles.data(), triangleCount, areas.data());
	for (int i = 0; i < triangleCount; ++i)
		if (!masks[i]) areas[i] = RC_NULL_AREA;

	if (!rcCreateHeightfield(&context, *field, config.width, config.height,
		config.bmin, config.bmax, config.cs, config.ch)) return false;
	if (!rcRasterizeTriangles(&context, vertices.data(), vertexCount, triangles.data(),
		areas.data(), triangleCount, *field, config.walkableClimb)) return false;

	// 段差・崖・頭上の空間はRecastの標準フィルターで判定する。
	rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *field);
	rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *field);
	if (!rcBuildCompactHeightfield(&context, config.walkableHeight, config.walkableClimb,
		*field, *compact)) return false;
	field.reset();

	// コライダーの範囲を歩行不可にし、その後でキャラクター半径分を縮小する。
	for (const ObstacleBounds& obstacle : obstacles)
	{
		const Vector3 half = obstacle.size * 0.5f;
		const float boundsMin[] = {obstacle.center.x - half.x,
			obstacle.center.y - half.y - agentHeight, obstacle.center.z - half.z};
		const float boundsMax[] = {obstacle.center.x + half.x,
			obstacle.center.y + half.y, obstacle.center.z + half.z};
		rcMarkBoxArea(&context, boundsMin, boundsMax, RC_NULL_AREA, *compact);
	}
	if (!rcErodeWalkableArea(&context, config.walkableRadius, *compact)) return false;
	if (!rcBuildDistanceField(&context, *compact)) return false;
	if (!rcBuildRegions(&context, *compact, config.borderSize,
		config.minRegionArea, config.mergeRegionArea)) return false;
	if (!rcBuildContours(&context, *compact, config.maxSimplificationError,
		config.maxEdgeLen, *contours)) return false;
	if (!rcBuildPolyMesh(&context, *contours, config.maxVertsPerPoly, *mesh)) return false;
	if (mesh->npolys && !rcBuildPolyMeshDetail(&context, *mesh, *compact,
		config.detailSampleDist, config.detailSampleMaxError, *detail)) return false;

	// ポリゴンの隣接情報とタイルポータルはRecastの生成結果をそのまま渡す。
	for (int i = 0; i < mesh->npolys; ++i)
	{
		mesh->areas[i] = 0;
		mesh->flags[i] = 1;
	}
	dtNavMeshCreateParams params{};
	params.verts = mesh->verts;
	params.vertCount = mesh->nverts;
	params.polys = mesh->polys;
	params.polyFlags = mesh->flags;
	params.polyAreas = mesh->areas;
	params.polyCount = mesh->npolys;
	params.nvp = mesh->nvp;
	params.detailMeshes = detail->meshes;
	params.detailVerts = detail->verts;
	params.detailVertsCount = detail->nverts;
	params.detailTris = detail->tris;
	params.detailTriCount = detail->ntris;
	params.tileX = tileX;
	params.tileY = tileZ;
	rcVcopy(params.bmin, mesh->bmin);
	rcVcopy(params.bmax, mesh->bmax);
	params.walkableHeight = agentHeight;
	params.walkableRadius = agentRadius;
	params.walkableClimb = agentMaxClimb;
	params.cs = config.cs;
	params.ch = config.ch;
	params.buildBvTree = true;

	unsigned char* data = nullptr;
	int dataSize = 0;
	if (mesh->npolys && !dtCreateNavMeshData(&params, &data, &dataSize)) return false;

	NavTile& tile = navTiles[tileZ * tileCountX + tileX];
	if (tile.reference)
	{
		if (dtStatusFailed(navMesh->removeTile(tile.reference, nullptr, nullptr)))
		{
			dtFree(data);
			return false;
		}
		tile.reference = 0;
	}
	if (!data) return true;
	if (dtStatusFailed(navMesh->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, &tile.reference)))
	{
		dtFree(data);
		return false;
	}
	return true;
}

void NavMeshActor::RebuildRegion()
{
	PROFILE_FUNCTION();
	if (!built || !navMesh)
	{
		Build();
		return;
	}

	Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr;
	if (!terrain)
	{
		statusMessage = "地形がないためナビメッシュを再生成できません";
		return;
	}

	const int grid = std::max(terrain->GetHeightMapWidth(), 2);
	const float terrainSize = terrain->GetTerrainSize();
	const float halfTerrainSize = terrainSize * 0.5f;
	const float tileSize = terrainSize / static_cast<float>(grid) * CellsPerTile;
	const float rebuildMargin = std::ceil(agentRadius / (terrainSize / grid)) *
		(terrainSize / grid) + 3.0f * terrainSize / grid;
	const int minTileX = std::clamp(static_cast<int>(floorf(
		(rebuildRegionMin.x - rebuildMargin + halfTerrainSize) / tileSize)), 0, tileCountX - 1);
	const int minTileZ = std::clamp(static_cast<int>(floorf(
		(rebuildRegionMin.z - rebuildMargin + halfTerrainSize) / tileSize)), 0, tileCountZ - 1);
	const int maxTileX = std::clamp(static_cast<int>(floorf(
		(rebuildRegionMax.x + rebuildMargin + halfTerrainSize) / tileSize)), 0, tileCountX - 1);
	const int maxTileZ = std::clamp(static_cast<int>(floorf(
		(rebuildRegionMax.z + rebuildMargin + halfTerrainSize) / tileSize)), 0, tileCountZ - 1);

	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);
	int rebuiltTileCount = 0;
	for (int tileZ = minTileZ; tileZ <= maxTileZ; ++tileZ)
	{
		for (int tileX = minTileX; tileX <= maxTileX; ++tileX)
		{
			if (!BuildTile(tileX, tileZ, *terrain, obstacles))
			{
				statusMessage = "NavMeshタイルの局所再生成に失敗しました";
				return;
			}
			++rebuiltTileCount;
		}
	}

	RefreshDebugCells();
	statusMessage = "局所再生成完了  タイル=" +
		std::to_string(rebuiltTileCount);
}

void NavMeshActor::RefreshDebugCells()
{
	debugCells.clear();
	if (!navMesh) return;
	const dtNavMesh& mesh = *navMesh;
	for (int tileIndex = 0; tileIndex < mesh.getMaxTiles(); ++tileIndex)
	{
		const dtMeshTile* tile = mesh.getTile(tileIndex);
		if (!tile || !tile->header) continue;
		for (int i = 0; i < tile->header->polyCount; ++i)
		{
			const dtPoly& polygon = tile->polys[i];
			if (!(polygon.flags & 1) || polygon.getType() != DT_POLYTYPE_GROUND) continue;
			for (int vertex = 1; vertex + 1 < polygon.vertCount; ++vertex)
			{
				DebugCell cell;
				cell.walkable = true;
				const int indices[] = {0, vertex, vertex + 1};
				for (int corner = 0; corner < 3; ++corner)
				{
					const float* position = &tile->verts[polygon.verts[indices[corner]] * 3];
					cell.corners[corner] = Vector3(position[0], position[1], position[2]);
				}
				debugCells.push_back(cell);
			}
		}
	}
}

bool NavMeshActor::FindNextPoint(
	const Vector3& start, const Vector3& goal, Vector3& nextPoint,
	Vector3* reachableGoal) const
{
	PROFILE_FUNCTION();
	if (!built || !navQuery) return false;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);

	const float extents[] = {nearestPolyExtent, 20.0f, nearestPolyExtent};
	const Vector3 navigationStart = ProjectTerrainPosition(start, true);
	const Vector3 navigationGoal = ProjectTerrainPosition(goal, true);
	const float startPos[] = {navigationStart.x, navigationStart.y, navigationStart.z};
	const float goalPos[] = {navigationGoal.x, navigationGoal.y, navigationGoal.z};
	float nearestStart[3] = {};
	float nearestGoal[3] = {};
	dtPolyRef startRef = 0;
	dtPolyRef goalRef = 0;

	if (dtStatusFailed(navQuery->findNearestPoly(
		startPos, extents, &filter, &startRef, nearestStart)) || !startRef) return false;
	if (dtStatusFailed(navQuery->findNearestPoly(
		goalPos, extents, &filter, &goalRef, nearestGoal)) || !goalRef) return false;

	// Detour標準の経路探索で、次に進む曲がり角だけを求める。
	constexpr int maxPathPoints = 256;
	dtPolyRef path[maxPathPoints] = {};
	int pathCount = 0;
	if (dtStatusFailed(navQuery->findPath(startRef, goalRef,
		nearestStart, nearestGoal, &filter, path, &pathCount, maxPathPoints)) || !pathCount) return false;

	float pathEnd[3] = {};
	if (dtStatusFailed(navQuery->closestPointOnPoly(
		path[pathCount - 1], nearestGoal, pathEnd, nullptr))) return false;

	float points[maxPathPoints * 3] = {};
	int pointCount = 0;
	if (dtStatusFailed(navQuery->findStraightPath(nearestStart, pathEnd,
		path, pathCount, points, nullptr, nullptr, &pointCount, maxPathPoints)) || !pointCount) return false;

	if (reachableGoal) *reachableGoal = Vector3(pathEnd[0], pathEnd[1], pathEnd[2]);
	nextPoint = Vector3(pathEnd[0], pathEnd[1], pathEnd[2]);
	for (int i = 1; i < pointCount; ++i)
	{
		const Vector3 point(points[i * 3], points[i * 3 + 1], points[i * 3 + 2]);
		Vector3 distance = point - start;
		distance.y = 0.0f;
		if (distance.LengthSquared() <= 0.01f) continue;
		nextPoint = point;
		break;
	}
	return true;
}

Vector3 NavMeshActor::ProjectTerrainPosition(const Vector3& position, bool navigationBase) const
{
	Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr;
	if (!terrain || terrain->GetTerrainSize() <= 0.0f) return position;
	const float size = terrain->GetTerrainSize();
	return Vector3(position.x,
		terrain->GetSurfaceHeightByUV(position.x / size + 0.5f, position.z / size + 0.5f, navigationBase),
		position.z);
}

bool NavMeshActor::FindNearestPoint(const Vector3& position, Vector3& nearestPoint) const
{
	if (!built || !navQuery) return false;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	filter.setExcludeFlags(0);

	const float horizontalExtent = nearestPolyExtent;
	const float halfExtents[3] = {horizontalExtent, 20.0f, horizontalExtent};
	const Vector3 navigationPosition = ProjectTerrainPosition(position, true);
	const float queryPosition[3] = {navigationPosition.x, navigationPosition.y, navigationPosition.z};
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

bool NavMeshActor::IsOutsideOrNearBoundary(const Vector3& position, float distance) const
{
	if (!built || !navQuery) return true;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	const float extent = nearestPolyExtent;
	const float halfExtents[] = {extent, 20.0f, extent};
	const Vector3 navigationPosition = ProjectTerrainPosition(position, true);
	const float queryPosition[] = {navigationPosition.x, navigationPosition.y, navigationPosition.z};
	float nearestPosition[3] = {};
	dtPolyRef reference = 0;
	if (dtStatusFailed(navQuery->findNearestPoly(queryPosition, halfExtents,
		&filter, &reference, nearestPosition)) || !reference) return true;

	// 足元の高さ差ではなく、XZ平面でNavMeshの外側か判定する。
	const float dx = nearestPosition[0] - position.x;
	const float dz = nearestPosition[2] - position.z;
	if (dx * dx + dz * dz > 0.0001f) return true;
	distance = std::max(distance, 0.0f);
	if (distance <= 0.0f) return false;
	float wallDistance = distance;
	float wallPosition[3] = {};
	float wallNormal[3] = {};
	if (dtStatusFailed(navQuery->findDistanceToWall(reference, nearestPosition, distance,
		&filter, &wallDistance, wallPosition, wallNormal))) return true;
	return wallDistance < distance;
}

bool NavMeshActor::FindRecoveryPoint(
	const Vector3& position, float safeDistance, Vector3& recoveryPoint) const
{
	if (!built || !navQuery) return false;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	filter.setExcludeFlags(0);

	const float horizontalExtent = nearestPolyExtent;
	const float halfExtents[3] = {horizontalExtent, 20.0f, horizontalExtent};
	const Vector3 navigationPosition = ProjectTerrainPosition(position, true);
	const float queryPosition[3] = {navigationPosition.x, navigationPosition.y, navigationPosition.z};
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

	const float horizontalExtent = nearestPolyExtent;
	const float halfExtents[3] = {horizontalExtent, 20.0f, horizontalExtent};
	const Vector3 navigationCenter = ProjectTerrainPosition(center, true);
	const float centerPos[3] = {navigationCenter.x, navigationCenter.y, navigationCenter.z};
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
		const Vector3 navigationCandidate = ProjectTerrainPosition(Vector3(
			center.x + sinf(angle) * distance, center.y, center.z + cosf(angle) * distance), true);
		const float candidate[3] = {navigationCandidate.x, navigationCandidate.y, navigationCandidate.z};

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

bool NavMeshActor::IsCliffAlongSegment(
	const Vector3& start, const Vector3& goal, float maxSlope) const
{
	Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr;
	if (!terrain) return false;
	const Transform* transform = owner->GetTransform();
	const Matrix world = transform ? transform->GetMatrix() : Matrix::Identity;
	const Matrix inverse = world.Invert();
	const Vector3 localStart = Vector3::Transform(start, inverse);
	const Vector3 localGoal = Vector3::Transform(goal, inverse);
	const float size = terrain->GetTerrainSize();
	Vector3 delta = localGoal - localStart;
	delta.y = 0.0f;
	const float length = delta.Length();
	if (length <= eps || size <= 0.0f) return false;
	const float spacing = size / std::max(terrain->GetHeightMapWidth(), 1) * 0.5f;
	const int samples = std::max(static_cast<int>(ceilf(length / spacing)), 1);
	const float maxRise = tanf(RAD(std::clamp(maxSlope, 0.0f, 89.0f)));
	Vector3 previous = localStart;
	previous.y = terrain->GetSurfaceHeightByUV(previous.x / size + 0.5f, previous.z / size + 0.5f, true);
	previous = Vector3::Transform(previous, world);
	for (int i = 1; i <= samples; ++i)
	{
		Vector3 point = localStart + delta * (static_cast<float>(i) / samples);
		const float u = point.x / size + 0.5f;
		const float v = point.z / size + 0.5f;
		if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return true;
		point.y = terrain->GetSurfaceHeightByUV(u, v, true);
		point = Vector3::Transform(point, world);
		const float horizontal = Vector2(point.x - previous.x, point.z - previous.z).Length();
		if (point.y - previous.y > horizontal * maxRise + 0.0001f) return true;
		previous = point;
	}
	return false;
}

bool NavMeshActor::IsDirectPathBlocked(
	const Vector3& start, const Vector3& goal, float radius, float height) const
{

	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);

	const Vector3 delta = goal - start;
	for (const ObstacleBounds& obstacle : obstacles)
	{
		const float lowestFoot = std::min(start.y, goal.y);
		const float highestHead = std::max(start.y, goal.y) + height;
		if (height > 0.0f && (highestHead <= obstacle.center.y - obstacle.size.y * 0.5f ||
			lowestFoot >= obstacle.center.y + obstacle.size.y * 0.5f)) continue;
		const Vector3 halfSize = obstacle.size * 0.5f + Vector3(radius, 0.0f, radius);
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
	const Vector3& start, const Vector3& goal, Vector3& nextPoint, float radius, float height) const
{
	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);

	auto intersects = [radius, height](const Vector3& from, const Vector3& to, const ObstacleBounds& obstacle) {
		if (height > 0.0f && (std::max(from.y, to.y) + height <= obstacle.center.y - obstacle.size.y * 0.5f ||
			std::min(from.y, to.y) >= obstacle.center.y + obstacle.size.y * 0.5f)) return false;
		const Vector3 halfSize = obstacle.size * 0.5f + Vector3(radius, 0.0f, radius);
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
	const float clearance = std::max(radius, 0.1f);
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
	Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr;
	const auto* brushPreview = rc.renderSettings.terrainBrushPreview;
	const bool preview = terrain && brushPreview && brushPreview->terrain == terrain && brushPreview->mode == 5;
	if (!preview && (!rc.renderSettings.showDebug || !rc.renderSettings.showNavMeshDebug)) return;

	PrimitiveRenderer* renderer = Game::Graphics::Instance().GetPrimitiveRenderer();
	if (!renderer) return;

	const Color walkableColor(0.0f, 0.75f, 1.0f, 0.30f);
	const Color blockedColor(1.0f, 0.15f, 0.05f, 0.65f);

	for (int index = 0; index < static_cast<int>(debugCells.size()); ++index)
	{
		const DebugCell& cell = debugCells[index];
		const Color color = cell.walkable ? (preview ? Color(0.1f, 0.8f, 1.0f, 0.8f) : walkableColor) : blockedColor;
		if (cell.walkable && !preview)
		{
			renderer->DrawTriangle(cell.corners[0], cell.corners[1], cell.corners[2], color);
			continue;
		}
		renderer->DrawLine(cell.corners[0], cell.corners[1], color, color);
		renderer->DrawLine(cell.corners[1], cell.corners[2], color, color);
		renderer->DrawLine(cell.corners[2], cell.corners[0], color, color);
	}

}

void NavMeshActor::DrawGUI()
{
	if (Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr)
		ImGui::Text("入力：NavMesh範囲マップ %d × %d",
			terrain->GetHeightMapWidth(), terrain->GetHeightMapHeight());

	bool simplifyChanged = ImGui::Checkbox("平面のポリゴンを削減", &simplifyFlatAreas);
	if (simplifyFlatAreas)
	{
		simplifyChanged |= ImGui::DragFloat("高さの許容誤差", &simplifyHeightError,
			0.001f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		simplifyChanged |= ImGui::DragInt("統合する最大幅（画素）", &simplifyMaxCells,
			1.0f, 1, CellsPerTile, "%d", ImGuiSliderFlags_AlwaysClamp);
		ImGui::TextDisabled("誤差・最大幅を大きくすると削減量が増えます");
	}
	if (simplifyChanged) RequestBuild();
	ImGui::Text("生成ポリゴン数：%zu", debugCells.size());

	// 生成状態と手動再生成
	ImGui::Text("%s", statusMessage.c_str());

	if (ImGui::Button("ナビメッシュを再生成")) buildRequested = true;
}
