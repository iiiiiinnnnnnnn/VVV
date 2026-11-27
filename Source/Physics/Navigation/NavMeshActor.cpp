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
#include "Physics/Navigation/NavMeshBuilder.h"
#include "DetourStatus.h"

#include <cmath>
#include <cstdlib>

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

	// 障害物の移動・追加・削除は、影響するタイルだけを再生成する。
	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);
	const size_t count = std::max(obstacles.size(), previousObstacles.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (i < obstacles.size() && i < previousObstacles.size() &&
			obstacles[i].center == previousObstacles[i].center &&
			obstacles[i].size == previousObstacles[i].size) continue;

		if (i < previousObstacles.size())
			RequestBuildRegion(previousObstacles[i].center, previousObstacles[i].size.Length() * 0.5f, 0);
		if (i < obstacles.size())
			RequestBuildRegion(obstacles[i].center, obstacles[i].size.Length() * 0.5f, 0);
	}
	previousObstacles = std::move(obstacles);
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
	root["generationVersion"] = 4;
	root["simplifyFlatAreas"] = simplifyFlatAreas;
	root["simplifyHeightError"] = simplifyHeightError;
	root["simplifyMaxCells"] = simplifyMaxCells;
	root["agentHeight"] = agentHeight;
	root["agentRadius"] = agentRadius;
	root["agentMaxClimb"] = agentMaxClimb;
	root["agentMaxSlope"] = agentMaxSlope;
	root["contourMaxError"] = contourMaxError;
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

		// 旧ステージにはRecast設定がないため、既定値で移行する。
		agentHeight = std::clamp(root.value("agentHeight", 2.0f), 0.1f, 20.0f);
		agentRadius = std::clamp(root.value("agentRadius", 0.4f), 0.0f, 10.0f);
		agentMaxClimb = std::clamp(root.value("agentMaxClimb", 0.3f), 0.0f, agentHeight);
		agentMaxSlope = std::clamp(root.value("agentMaxSlope", 45.0f), 0.0f, 89.0f);
		contourMaxError = std::clamp(root.value("contourMaxError", 1.3f), 0.0f, 10.0f);
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

	if (navMaxY - navMinY < 0.1f)
	{
		statusMessage = "生成する高さの範囲を0.1以上にしてください";
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
	NavMeshBuilder::BuildSettings settings;
	settings.cellSize = size / grid;
	settings.cellHeight = 0.05f;
	settings.agentHeight = agentHeight;
	settings.agentRadius = agentRadius;
	settings.agentMaxClimb = agentMaxClimb;
	settings.agentMaxSlope = agentMaxSlope;
	settings.maxEdgeLength = simplifyFlatAreas ? simplifyMaxCells : 1;
	settings.contourMaxError = simplifyFlatAreas ? contourMaxError : 0.0f;
	settings.detailSampleDistance = std::max(0.9f, settings.cellSize * 6.0f);
	settings.detailMaxError = simplifyFlatAreas ? simplifyHeightError : 0.0f;

	NavMeshBuilder::Tile buildTile;
	buildTile.origin = {-halfSize, 0.0f, -halfSize};
	buildTile.x = tileX;
	buildTile.z = tileZ;
	buildTile.cellCount = CellsPerTile;
	buildTile.minY = navMinY;
	buildTile.maxY = navMaxY;
	const int border = NavMeshBuilder::GetBorderSize(settings);
	const int minX = std::max(tileX * CellsPerTile - border, 0);
	const int minZ = std::max(tileZ * CellsPerTile - border, 0);
	const int maxX = std::min((tileX + 1) * CellsPerTile + border, grid);
	const int maxZ = std::min((tileZ + 1) * CellsPerTile + border, grid);

	// 描画と同じ補間と変形前の高さを使い、以前と同じ入力値を用意する。
	std::vector<float> heights;
	std::vector<uint8_t> masks;
	heights.reserve(static_cast<size_t>(maxX - minX + 1) * (maxZ - minZ + 1));
	masks.reserve(static_cast<size_t>(maxX - minX) * (maxZ - minZ));
	for (int z = minZ; z <= maxZ; ++z)
	{
		for (int x = minX; x <= maxX; ++x)
		{
			const float u = static_cast<float>(x) / grid;
			const float v = static_cast<float>(z) / grid;
			heights.push_back(terrain.GetSurfaceHeightByUV(u, v, true));
		}
	}
	for (int z = minZ; z < maxZ; ++z)
		for (int x = minX; x < maxX; ++x)
			masks.push_back(terrain.GetNavMeshMaskByUV((x + 0.5f) / grid, (z + 0.5f) / grid) >= 0.5f);

	NavMeshBuilder::HeightMap input;
	input.heights = heights;
	input.walkableMask = masks;
	input.width = maxX - minX + 1;
	input.height = maxZ - minZ + 1;
	input.originX = (static_cast<float>(minX) / grid - 0.5f) * size;
	input.originZ = (static_cast<float>(minZ) / grid - 0.5f) * size;
	input.spacingX = settings.cellSize;
	input.spacingZ = settings.cellSize;
	std::vector<NavMeshBuilder::Bounds> obstacleBounds;
	obstacleBounds.reserve(obstacles.size());
	for (const ObstacleBounds& obstacle : obstacles)
	{
		const Vector3 min = obstacle.center - obstacle.size * 0.5f;
		const Vector3 max = obstacle.center + obstacle.size * 0.5f;
		obstacleBounds.push_back({{min.x, min.y, min.z}, {max.x, max.y, max.z}});
	}

	NavMeshBuilder::TileData result;
	if (!NavMeshBuilder::BuildTile(input, settings, buildTile, obstacleBounds, result)) return false;

	NavTile& tile = navTiles[tileZ * tileCountX + tileX];
	if (tile.reference)
	{
		if (dtStatusFailed(navMesh->removeTile(tile.reference, nullptr, nullptr))) return false;
		tile.reference = 0;
	}
	if (!result.data) return true;
	if (dtStatusFailed(navMesh->addTile(result.data.get(), result.size,
		DT_TILE_FREE_DATA, 0, &tile.reference))) return false;

	// 登録に成功したデータだけ、Detourへ所有権を移す。
	result.data.release();
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
	NavMeshBuilder::BuildSettings settings;
	settings.cellSize = terrainSize / grid;
	settings.agentRadius = agentRadius;
	const float rebuildMargin = NavMeshBuilder::GetBorderSize(settings) * settings.cellSize;
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
			// Detourの詳細三角形を表示し、経路が参照する地表の高さと揃える。
			const dtPolyDetail& detail = tile->detailMeshes[i];
			for (int triangle = 0; triangle < detail.triCount; ++triangle)
			{
				DebugCell cell;
				cell.walkable = true;
				const unsigned char* indices = &tile->detailTris[(detail.triBase + triangle) * 4];
				for (int corner = 0; corner < 3; ++corner)
				{
					const int vertex = indices[corner];
					const float* position = vertex < polygon.vertCount
						? &tile->verts[polygon.verts[vertex] * 3]
						: &tile->detailVerts[(detail.vertBase + vertex - polygon.vertCount) * 3];
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
	const Vector3 target = recoveryPoint + inward * (safeDistance - wallDistance);
	const float end[] = {target.x, target.y, target.z};
	float result[3] = {};
	dtPolyRef visited[32] = {};
	int visitedCount = 0;
	if (dtStatusFailed(navQuery->moveAlongSurface(nearestRef, nearestPosition, end,
		&filter, result, visited, &visitedCount, _countof(visited)))) return false;
	if (!visitedCount) return false;
	if (dtStatusFailed(navQuery->getPolyHeight(visited[visitedCount - 1], result, &result[1]))) return false;
	recoveryPoint = Vector3(result[0], result[1], result[2]);
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
		dtPolyRef goalRef = 0;
		float nearestGoal[3] = {};
		const auto random = []() -> float
		{
			return static_cast<float>(std::rand() / (RAND_MAX + 1.0));
		};
		if (dtStatusFailed(navQuery->findRandomPointAroundCircle(centerRef, nearestCenter,
			maxDistance, &filter, random, &goalRef, nearestGoal)) || !goalRef) continue;

		const Vector3 offset(nearestGoal[0] - center.x, 0.0f, nearestGoal[2] - center.z);
		const float distanceSq = offset.LengthSquared();
		if (distanceSq < minDistanceSq || distanceSq > maxDistanceSq) continue;

		randomPoint = Vector3(nearestGoal[0], nearestGoal[1], nearestGoal[2]);
		return true;
	}

	return false;
}

// 突進の停止条件。NavMeshの塗装範囲とは分け、指定角度の上り坂だけを調べる。
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

bool NavMeshActor::IsDirectPathBlocked(const Vector3& start, const Vector3& goal) const
{
	if (!built || !navQuery) return true;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	const float extents[] = {nearestPolyExtent, 20.0f, nearestPolyExtent};
	const Vector3 navigationStart = ProjectTerrainPosition(start, true);
	const Vector3 navigationGoal = ProjectTerrainPosition(goal, true);
	const float origin[] = {navigationStart.x, navigationStart.y, navigationStart.z};
	const float end[] = {navigationGoal.x, navigationGoal.y, navigationGoal.z};
	float nearestStart[3] = {};
	dtPolyRef startRef = 0;
	if (dtStatusFailed(navQuery->findNearestPoly(origin, extents, &filter,
		&startRef, nearestStart)) || !startRef) return true;
	if (Vector2(nearestStart[0] - start.x, nearestStart[2] - start.z).LengthSquared() > 0.0001f)
		return true;

	// 半径と高さの余白はRecast生成時に確保済み。直線上の壁はDetourで調べる。
	float hit = 0.0f;
	float normal[3] = {};
	if (dtStatusFailed(navQuery->raycast(startRef, nearestStart, end, &filter,
		&hit, normal, nullptr, nullptr, 0))) return true;
	return hit < 1.0f;
}

bool NavMeshActor::FindObstacleDetourPoint(
	const Vector3& start, const Vector3& goal, Vector3& nextPoint) const
{
	if (!IsDirectPathBlocked(start, goal)) return false;
	return FindNextPoint(start, goal, nextPoint);
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

	// キャラクターの通行条件はRecastの生成設定に統一する。
	bool generationChanged = ImGui::DragFloat("エージェントの高さ", &agentHeight,
		0.05f, 0.1f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	generationChanged |= ImGui::DragFloat("エージェントの半径", &agentRadius,
		0.05f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	generationChanged |= ImGui::DragFloat("登れる段差", &agentMaxClimb,
		0.05f, 0.0f, agentHeight, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	generationChanged |= ImGui::SliderFloat("歩行可能な最大傾斜", &agentMaxSlope, 0.0f, 89.0f);
	generationChanged |= ImGui::DragFloatRange2("生成する高さの範囲", &navMinY, &navMaxY, 1.0f);
	if (generationChanged) RequestBuild();

	bool simplifyChanged = ImGui::Checkbox("平面のポリゴンを削減", &simplifyFlatAreas);
	if (simplifyFlatAreas)
	{
		simplifyChanged |= ImGui::DragFloat("詳細メッシュの高さ誤差", &simplifyHeightError,
			0.001f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		simplifyChanged |= ImGui::DragFloat("輪郭の許容誤差（セル）", &contourMaxError,
			0.1f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		simplifyChanged |= ImGui::DragInt("輪郭辺の最大長（セル）", &simplifyMaxCells,
			1.0f, 1, CellsPerTile, "%d", ImGuiSliderFlags_AlwaysClamp);
		ImGui::TextDisabled("誤差・最大幅を大きくすると削減量が増えます");
	}
	if (simplifyChanged) RequestBuild();
	int polygonCount = 0;
	if (navMesh)
	{
		const dtNavMesh& mesh = *navMesh;
		for (int i = 0; i < mesh.getMaxTiles(); ++i)
		{
			const dtMeshTile* tile = mesh.getTile(i);
			if (tile && tile->header) polygonCount += tile->header->polyCount;
		}
	}
	ImGui::Text("生成ポリゴン数：%d", polygonCount);

	// 生成状態と手動再生成
	ImGui::Text("%s", statusMessage.c_str());

	if (ImGui::Button("ナビメッシュを再生成")) RequestBuild();
}
