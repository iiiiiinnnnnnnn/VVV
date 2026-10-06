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

#include <cmath>
#include <map>
#include <queue>
#include <functional>
#include <set>

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
	if (text.empty())
	{
		RequestBuild();
		return true;
	}

	try
	{
		const json root = json::parse(text);
		simplifyFlatAreas = root.value("simplifyFlatAreas", simplifyFlatAreas);
		simplifyHeightError = std::clamp(root.value("simplifyHeightError", simplifyHeightError), 0.0f, 1.0f);
		simplifyMaxCells = std::clamp(root.value("simplifyMaxCells", simplifyMaxCells), 1, CellsPerTile);
		navMinY = root.value("navMinY", navMinY);
		navMaxY = root.value("navMaxY", navMaxY);
		if (navMinY > navMaxY) std::swap(navMinY, navMaxY);

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
	for (int tileZ = 0; tileZ < tileCountZ; ++tileZ)
	{
		for (int tileX = 0; tileX < tileCountX; ++tileX)
		{
			if (!BuildTile(tileX, tileZ, *terrain))
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
		std::to_string(tileCountX * tileCountZ);
}

bool NavMeshActor::BuildTile(
	int tileX,
	int tileZ,
	Terrain& terrain)
{
	const int grid = std::max(terrain.GetHeightMapWidth(), 2);
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
				terrain.GetSurfaceHeightByUV(u, v),
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

	for (int localZ = 0; localZ < cellCountZ; ++localZ)
	{
		for (int localX = 0; localX < cellCountX; ++localX)
		{
			const int gridX = minGridX + localX;
			const int gridZ = minGridZ + localZ;
			const float u = (static_cast<float>(gridX) + 0.5f) / static_cast<float>(grid);
			const float v = (static_cast<float>(gridZ) + 0.5f) / static_cast<float>(grid);
			// Each green mask pixel owns one cell; adjacent pixels share the same edge vertices.
			if (terrain.GetNavMeshMaskByUV(u, v) < 0.5f) continue;

			const unsigned short i0 =
				static_cast<unsigned short>(localZ * (cellCountX + 1) + localX);
			const unsigned short i1 = i0 + 1;
			const unsigned short i2 = i0 + static_cast<unsigned short>(cellCountX + 1);
			const unsigned short i3 = i2 + 1;
			const Vector3& p0 = worldVertices[i0];
			const Vector3& p1 = worldVertices[i1];
			const Vector3& p2 = worldVertices[i2];
			const Vector3& p3 = worldVertices[i3];

			const bool firstWalkable = (p2 - p0).Cross(p1 - p0).LengthSquared() > eps;
			const bool secondWalkable = (p2 - p1).Cross(p3 - p1).LengthSquared() > eps;

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

	if (simplifyFlatAreas)
	{
		struct Triangle
		{
			unsigned short vertices[3];
			bool alive = true;
		};
		std::vector<Triangle> triangles;
		std::vector<std::vector<size_t>> incident(worldVertices.size());
		for (size_t i = 0; i < flags.size(); ++i)
		{
			const unsigned short* polygon = &polys[i * nvp * 2];
			triangles.push_back({{polygon[0], polygon[1], polygon[2]}, true});
			for (unsigned short vertex : triangles.back().vertices) incident[vertex].push_back(i);
		}
		const float maxEdge = cellSize * simplifyMaxCells;
		const float maxEdgeSq = maxEdge * maxEdge;
		auto horizontalArea = [&](const Triangle& triangle) {
			const Vector3& a = worldVertices[triangle.vertices[0]];
			const Vector3& b = worldVertices[triangle.vertices[1]];
			const Vector3& c = worldVertices[triangle.vertices[2]];
			return (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
		};
		// Collapse only interior vertices. Mask boundaries and tile seams keep their original edges.
		for (unsigned short vertex = 0; vertex < worldVertices.size(); ++vertex)
		{
			const int x = vertex % (cellCountX + 1);
			const int z = vertex / (cellCountX + 1);
			if (!x || !z || x == cellCountX || z == cellCountZ) continue;
			std::vector<size_t> fan;
			std::map<unsigned short, int> neighbors;
			for (size_t index : incident[vertex])
			{
				const Triangle& triangle = triangles[index];
				if (!triangle.alive) continue;
				fan.push_back(index);
				for (unsigned short other : triangle.vertices)
					if (other != vertex) ++neighbors[other];
			}
			if (fan.size() < 3) continue;
			bool interior = true;
			for (const auto& [other, count] : neighbors)
				if (count != 2) { interior = false; break; }
			if (!interior) continue;
			const Triangle& basis = triangles[fan.front()];
			const Vector3& origin = worldVertices[basis.vertices[0]];
			const Vector3 normal = (worldVertices[basis.vertices[1]] - origin).Cross(
				worldVertices[basis.vertices[2]] - origin);
			if (fabsf(normal.y) <= eps) continue;
			bool planar = true;
			for (const auto& [other, count] : neighbors)
				if (fabsf(normal.Dot(worldVertices[other] - origin) / normal.y) > simplifyHeightError + 0.00001f)
				{ planar = false; break; }
			if (!planar) continue;
			for (const auto& [target, count] : neighbors)
			{
				std::set<unsigned short> targetNeighbors;
				for (size_t index : incident[target])
					if (triangles[index].alive)
						for (unsigned short other : triangles[index].vertices)
							if (other != target) targetNeighbors.insert(other);
				int common = 0;
				for (const auto& [other, degree] : neighbors)
					if (targetNeighbors.contains(other)) ++common;
				if (common != 2) continue;
				std::vector<Triangle> replacement;
				bool valid = true;
				for (size_t index : fan)
				{
					Triangle triangle = triangles[index];
					bool containsTarget = false;
					for (unsigned short other : triangle.vertices)
						if (other == target) containsTarget = true;
					if (containsTarget) continue;
					for (unsigned short& other : triangle.vertices)
						if (other == vertex) other = target;
					const float area = horizontalArea(triangle);
					if (fabsf(area) <= eps || area * horizontalArea(triangles[index]) <= 0.0f)
					{ valid = false; break; }
					for (int edge = 0; edge < 3; ++edge)
					{
						Vector3 delta = worldVertices[triangle.vertices[edge]] - worldVertices[triangle.vertices[(edge + 1) % 3]];
						delta.y = 0.0f;
						if (delta.LengthSquared() > maxEdgeSq) { valid = false; break; }
					}
					if (!valid) break;
					// Check against the original samples so repeated collapses cannot accumulate height error.
					const Vector3& a = worldVertices[triangle.vertices[0]];
					const Vector3& b = worldVertices[triangle.vertices[1]];
					const Vector3& c = worldVertices[triangle.vertices[2]];
					int minX = cellCountX, minZ = cellCountZ, maxX = 0, maxZ = 0;
					for (unsigned short other : triangle.vertices)
					{
						minX = std::min(minX, other % (cellCountX + 1));
						maxX = std::max(maxX, other % (cellCountX + 1));
						minZ = std::min(minZ, other / (cellCountX + 1));
						maxZ = std::max(maxZ, other / (cellCountX + 1));
					}
					for (int z = minZ; z <= maxZ && valid; ++z)
						for (int x = minX; x <= maxX; ++x)
						{
							const Vector3& sample = worldVertices[z * (cellCountX + 1) + x];
							const float u = ((sample.x - a.x) * (c.z - a.z) - (sample.z - a.z) * (c.x - a.x)) / area;
							const float v = ((b.x - a.x) * (sample.z - a.z) - (b.z - a.z) * (sample.x - a.x)) / area;
							if (u < -0.00001f || v < -0.00001f || u + v > 1.00001f) continue;
							if (fabsf(sample.y - (a.y + u * (b.y - a.y) + v * (c.y - a.y))) > simplifyHeightError + 0.00001f)
							{ valid = false; break; }
						}
					if (!valid) break;
					replacement.push_back(triangle);
				}
				if (!valid) continue;
				for (size_t index : fan) triangles[index].alive = false;
				for (const Triangle& triangle : replacement)
				{
					const size_t index = triangles.size();
					triangles.push_back(triangle);
					for (unsigned short other : triangle.vertices) incident[other].push_back(index);
				}
				break;
			}
		}
		polys.clear();
		flags.clear();
		areas.clear();
		for (const Triangle& triangle : triangles)
		{
			if (!triangle.alive) continue;
			const size_t offset = polys.size();
			polys.resize(offset + nvp * 2, 0xffff);
			for (int i = 0; i < 3; ++i) polys[offset + i] = triangle.vertices[i];
			flags.push_back(1);
			areas.push_back(0);
		}
	}

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
	params.walkableHeight = 0.0f;
	params.walkableRadius = 0.0f;
	params.walkableClimb = navMaxY - navMinY;
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

	const int grid = std::max(terrain->GetHeightMapWidth(), 2);
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

	int rebuiltTileCount = 0;
	for (int tileZ = minTileZ; tileZ <= maxTileZ; ++tileZ)
	{
		for (int tileX = minTileX; tileX <= maxTileX; ++tileX)
		{
			if (!BuildTile(tileX, tileZ, *terrain))
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
		for (const DebugCell& cell : tile.debugCells)
			if (!cell.walkable) debugCells.push_back(cell);
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
	const Vector3& start, const Vector3& goal, Vector3& nextPoint, Vector3* reachableGoal, float radius, float height) const
{
	if (!built || !navQuery) return false;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	filter.setExcludeFlags(0);

	const float horizontalExtent = nearestPolyExtent;
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

	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);
	const dtNavMesh& mesh = *navMesh;
	radius = std::max(radius, 0.0f);
	height = std::max(height, 0.0f);
	auto pointFits = [&](dtPolyRef reference, const Vector3& point) {
		for (const ObstacleBounds& obstacle : obstacles)
		{
			const Vector3 half = obstacle.size * 0.5f;
			if (point.y + height <= obstacle.center.y - half.y || point.y >= obstacle.center.y + half.y) continue;
			const float dx = std::max(fabsf(point.x - obstacle.center.x) - half.x, 0.0f);
			const float dz = std::max(fabsf(point.z - obstacle.center.z) - half.z, 0.0f);
			if (dx * dx + dz * dz <= radius * radius) return false;
		}
		if (radius <= 0.0f) return true;
		const float position[] = {point.x, point.y, point.z};
		float distance = radius;
		float hit[3] = {};
		float normal[3] = {};
		return dtStatusSucceed(navQuery->findDistanceToWall(reference, position, radius,
			&filter, &distance, hit, normal)) && distance >= radius - 0.001f;
	};
	auto segmentFits = [&](const Vector3& from, const Vector3& to) {
		const float spacing = std::max(radius * 0.5f, 0.1f);
		const int steps = std::max(static_cast<int>(ceilf(Vector3::Distance(from, to) / spacing)), 1);
		for (int i = 0; i <= steps; ++i)
		{
			const Vector3 point = Vector3::Lerp(from, to, static_cast<float>(i) / steps);
			const float position[] = {point.x, point.y, point.z};
			const float extent[] = {0.05f, 20.0f, 0.05f};
			float nearest[3] = {};
			dtPolyRef reference = 0;
			if (dtStatusFailed(navQuery->findNearestPoly(position, extent, &filter, &reference, nearest)) || !reference) return false;
			if (!pointFits(reference, Vector3(nearest[0], nearest[1], nearest[2]))) return false;
		}
		return true;
	};
	const Vector3 pathStart(nearestStart[0], nearestStart[1], nearestStart[2]);
	const Vector3 pathGoal(nearestGoal[0], nearestGoal[1], nearestGoal[2]);
	if (!pointFits(startRef, pathStart) || !pointFits(goalRef, pathGoal)) return false;
	if (segmentFits(pathStart, pathGoal))
	{
		nextPoint = pathGoal;
		if (reachableGoal) *reachableGoal = pathGoal;
		return true;
	}


	struct PathNode
	{
		Vector3 point = Vector3::Zero;
		float cost = FLT_MAX;
		dtPolyRef parent = 0;
		bool closed = false;
	};
	std::map<dtPolyRef, PathNode> nodes;
	auto nodeFor = [&](dtPolyRef reference) -> PathNode& {
		auto [it, inserted] = nodes.try_emplace(reference);
		if (inserted)
		{
			const dtMeshTile* tile = nullptr;
			const dtPoly* poly = nullptr;
			if (dtStatusSucceed(mesh.getTileAndPolyByRef(reference, &tile, &poly)))
			{
				for (int i = 0; i < poly->vertCount; ++i)
				{
					const float* vertex = &tile->verts[poly->verts[i] * 3];
					it->second.point += Vector3(vertex[0], vertex[1], vertex[2]);
				}
				it->second.point /= static_cast<float>(poly->vertCount);
			}
		}
		return it->second;
	};
	using Candidate = std::pair<float, dtPolyRef>;
	std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> pending;
	PathNode& first = nodeFor(startRef);
	first.point = pathStart;
	first.cost = 0.0f;
	nodeFor(goalRef).point = pathGoal;
	pending.emplace(Vector3::Distance(pathStart, pathGoal), startRef);
	int visited = 0;
	bool found = false;
	while (!pending.empty() && visited < 32768)
	{
		const dtPolyRef reference = pending.top().second;
		pending.pop();
		PathNode& current = nodeFor(reference);
		if (current.closed) continue;
		current.closed = true;
		++visited;
		if (reference == goalRef) { found = true; break; }
		const dtMeshTile* tile = nullptr;
		const dtPoly* poly = nullptr;
		if (dtStatusFailed(mesh.getTileAndPolyByRef(reference, &tile, &poly))) continue;
		for (unsigned int link = poly->firstLink; link != DT_NULL_LINK; link = tile->links[link].next)
		{
			const dtPolyRef neighborRef = tile->links[link].ref;
			if (!neighborRef) continue;
			PathNode& neighbor = nodeFor(neighborRef);
			if (neighbor.closed) continue;
			const float cost = current.cost + Vector3::Distance(current.point, neighbor.point);
			if (cost >= neighbor.cost || !segmentFits(current.point, neighbor.point)) continue;
			neighbor.cost = cost;
			neighbor.parent = reference;
			pending.emplace(cost + Vector3::Distance(neighbor.point, pathGoal), neighborRef);
		}
	}
	if (!found) return false;
	std::vector<Vector3> path;
	for (dtPolyRef reference = goalRef; reference; reference = nodes.at(reference).parent)
		path.push_back(nodes.at(reference).point);
	std::reverse(path.begin(), path.end());
	if (reachableGoal) *reachableGoal = pathGoal;
	if (path.size() == 1 && !segmentFits(pathStart, pathGoal)) return false;
	nextPoint = path.size() > 1 ? path[1] : pathGoal;
	// Keep the enemy's full footprint clear when smoothing the polygon route.
	for (size_t i = 1; i < path.size(); ++i)
	{
		if (!segmentFits(pathStart, path[i])) break;
		nextPoint = path[i];
	}

	return true;
}

bool NavMeshActor::FindNearestPoint(const Vector3& position, Vector3& nearestPoint) const
{
	if (!built || !navQuery) return false;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	filter.setExcludeFlags(0);

	const float horizontalExtent = nearestPolyExtent;
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

bool NavMeshActor::IsOutsideOrNearBoundary(const Vector3& position, float distance) const
{
	if (!built || !navQuery) return true;
	if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return true;

	dtQueryFilter filter;
	filter.setIncludeFlags(1);
	const float extent = nearestPolyExtent;
	const float halfExtents[] = {extent, 20.0f, extent};
	const float queryPosition[] = {position.x, position.y, position.z};
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

	const float horizontalExtent = nearestPolyExtent;
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

bool NavMeshActor::IsCliffAlongSegment(
	const Vector3& start, const Vector3& goal, float maxSlope) const
{
	Terrain* terrain = owner ? owner->GetComponent<Terrain>() : nullptr;
	if (!terrain) return false;
	const Transform* transform = owner->GetTransform();
	const Matrix world = transform ? transform->matrix : Matrix::Identity;
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
	const bool preview = terrain && terrain->IsNavMeshPreviewActive();
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
		ImGui::Text((const char*)u8"入力：NavMesh範囲マップ %d × %d",
			terrain->GetHeightMapWidth(), terrain->GetHeightMapHeight());

	bool simplifyChanged = ImGui::Checkbox((const char*)u8"平面のポリゴンを削減", &simplifyFlatAreas);
	if (simplifyFlatAreas)
	{
		simplifyChanged |= ImGui::DragFloat((const char*)u8"高さの許容誤差", &simplifyHeightError,
			0.001f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		simplifyChanged |= ImGui::DragInt((const char*)u8"統合する最大幅（画素）", &simplifyMaxCells,
			1.0f, 1, CellsPerTile, "%d", ImGuiSliderFlags_AlwaysClamp);
		ImGui::TextDisabled((const char*)u8"誤差・最大幅を大きくすると削減量が増えます");
	}
	if (simplifyChanged) RequestBuild();
	ImGui::Text((const char*)u8"生成ポリゴン数：%zu", debugCells.size());

	// 生成状態と手動再生成
	ImGui::Text("%s", statusMessage.c_str());

	if (ImGui::Button((const char*)u8"ナビメッシュを再生成")) buildRequested = true;
}
