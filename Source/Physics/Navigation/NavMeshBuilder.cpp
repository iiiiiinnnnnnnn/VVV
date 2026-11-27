#include "NavMeshBuilder.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Recast.h"
#include "DetourAlloc.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"

void NavMeshBuilder::DataDeleter::operator()(unsigned char* data) const
{
	dtFree(data);
}

int NavMeshBuilder::GetBorderSize(const BuildSettings& settings)
{
	return static_cast<int>(std::ceil(settings.agentRadius / settings.cellSize)) + 3;
}

NavMeshBuilder::BuildRegion NavMeshBuilder::GetTileRegion(const Tile& tile, const BuildSettings& settings)
{
	BuildRegion region;
	region.tileX = tile.x;
	region.tileZ = tile.z;
	region.borderSize = GetBorderSize(settings);
	region.bounds.min = {
		tile.origin[0] + (tile.x * tile.cellCount - region.borderSize) * settings.cellSize,
		tile.minY,
		tile.origin[2] + (tile.z * tile.cellCount - region.borderSize) * settings.cellSize};
	const float extent = (tile.cellCount + region.borderSize * 2) * settings.cellSize;
	region.bounds.max = {region.bounds.min[0] + extent, tile.maxY, region.bounds.min[2] + extent};
	return region;
}

bool NavMeshBuilder::BuildTile(const HeightMap& heightMap, const BuildSettings& settings,
	const Tile& tile, std::span<const Bounds> obstacles, TileData& result)
{
	if (settings.cellSize <= 0.0f || tile.cellCount <= 0)
	{
		result = {};
		return false;
	}
	return BuildFromHeightMap(heightMap, settings, GetTileRegion(tile, settings), obstacles, result);
}

bool NavMeshBuilder::BuildFromHeightMap(const HeightMap& heightMap, const BuildSettings& settings,
	const BuildRegion& region, std::span<const Bounds> obstacles, TileData& result)
{
	result = {};
	if (heightMap.width < 2 || heightMap.height < 2 ||
		heightMap.spacingX <= 0.0f || heightMap.spacingZ <= 0.0f) return false;
	const std::size_t vertexCount = static_cast<std::size_t>(heightMap.width) * heightMap.height;
	const std::size_t cellCount = static_cast<std::size_t>(heightMap.width - 1) * (heightMap.height - 1);
	if (heightMap.heights.size() != vertexCount ||
		(!heightMap.walkableMask.empty() && heightMap.walkableMask.size() != cellCount)) return false;

	// 入力の高さを変えずに、共通のメッシュ生成経路へ渡す。
	std::vector<float> vertices;
	std::vector<int> indices;
	std::vector<std::uint8_t> masks;
	vertices.reserve(vertexCount * 3);
	indices.reserve(cellCount * 6);
	masks.reserve(cellCount * 2);
	for (int z = 0; z < heightMap.height; ++z)
	{
		for (int x = 0; x < heightMap.width; ++x)
		{
			vertices.push_back(heightMap.originX + x * heightMap.spacingX);
			vertices.push_back(heightMap.heights[static_cast<std::size_t>(z) * heightMap.width + x]);
			vertices.push_back(heightMap.originZ + z * heightMap.spacingZ);
		}
	}
	for (int z = 0; z < heightMap.height - 1; ++z)
	{
		for (int x = 0; x < heightMap.width - 1; ++x)
		{
			const int i = z * heightMap.width + x;
			indices.insert(indices.end(), {i, i + heightMap.width, i + 1,
				i + 1, i + heightMap.width, i + heightMap.width + 1});
			const std::uint8_t mask = heightMap.walkableMask.empty() ? 1
				: heightMap.walkableMask[static_cast<std::size_t>(z) * (heightMap.width - 1) + x];
			masks.push_back(mask);
			masks.push_back(mask);
		}
	}
	return BuildFromMesh({vertices, indices, masks}, settings, region, obstacles, result);
}

bool NavMeshBuilder::BuildFromMesh(const Mesh& input, const BuildSettings& settings,
	const BuildRegion& region, std::span<const Bounds> obstacles, TileData& result)
{
	result = {};
	if (settings.cellSize <= 0.0f || settings.cellHeight <= 0.0f ||
		settings.agentHeight <= 0.0f || settings.agentRadius < 0.0f || settings.agentMaxClimb < 0.0f ||
		settings.agentMaxSlope < 0.0f || settings.agentMaxSlope >= 90.0f || region.borderSize < 0) return false;
	for (int axis = 0; axis < 3; ++axis)
		if (region.bounds.min[axis] >= region.bounds.max[axis]) return false;
	if (input.vertices.size() % 3 || input.indices.size() % 3 ||
		(!input.walkableTriangles.empty() && input.walkableTriangles.size() != input.indices.size() / 3))
		return false;
	for (int index : input.indices)
		if (index < 0 || static_cast<std::size_t>(index) >= input.vertices.size() / 3) return false;
	if (input.indices.empty()) return true;

	// 生成条件をRecastの単位へ変換する。タイル管理やゲーム設定はここへ持ち込まない。
	rcConfig config{};
	config.cs = settings.cellSize;
	config.ch = std::max(settings.cellHeight,
		(region.bounds.max[1] - region.bounds.min[1]) / (RC_SPAN_MAX_HEIGHT - 1));
	config.walkableSlopeAngle = settings.agentMaxSlope;
	config.walkableHeight = std::max(3, static_cast<int>(std::ceil(settings.agentHeight / config.ch)));
	config.walkableClimb = static_cast<int>(std::floor(settings.agentMaxClimb / config.ch));
	config.walkableRadius = static_cast<int>(std::ceil(settings.agentRadius / config.cs));
	config.borderSize = region.borderSize;
	config.maxEdgeLen = settings.maxEdgeLength;
	config.maxSimplificationError = settings.contourMaxError;
	config.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
	config.detailSampleDist = settings.detailSampleDistance;
	config.detailSampleMaxError = settings.detailMaxError;
	config.minRegionArea = settings.minRegionArea;
	config.mergeRegionArea = settings.mergeRegionArea;
	rcVcopy(config.bmin, region.bounds.min.data());
	rcVcopy(config.bmax, region.bounds.max.data());
	rcCalcGridSize(config.bmin, config.bmax, config.cs, &config.width, &config.height);
	if (config.width <= config.borderSize * 2 || config.height <= config.borderSize * 2) return false;

	rcContext context;
	std::unique_ptr<rcHeightfield, decltype(&rcFreeHeightField)> field(rcAllocHeightfield(), rcFreeHeightField);
	std::unique_ptr<rcCompactHeightfield, decltype(&rcFreeCompactHeightfield)> compact(
		rcAllocCompactHeightfield(), rcFreeCompactHeightfield);
	std::unique_ptr<rcContourSet, decltype(&rcFreeContourSet)> contours(rcAllocContourSet(), rcFreeContourSet);
	std::unique_ptr<rcPolyMesh, decltype(&rcFreePolyMesh)> mesh(rcAllocPolyMesh(), rcFreePolyMesh);
	std::unique_ptr<rcPolyMeshDetail, decltype(&rcFreePolyMeshDetail)> detail(rcAllocPolyMeshDetail(), rcFreePolyMeshDetail);
	if (!field || !compact || !contours || !mesh || !detail) return false;

	const int triangleCount = static_cast<int>(input.indices.size() / 3);
	const int vertexCount = static_cast<int>(input.vertices.size() / 3);
	std::vector<unsigned char> areas(triangleCount, RC_NULL_AREA);
	rcMarkWalkableTriangles(&context, config.walkableSlopeAngle,
		input.vertices.data(), vertexCount, input.indices.data(), triangleCount, areas.data());
	for (int i = 0; i < triangleCount; ++i)
		if (!input.walkableTriangles.empty() && !input.walkableTriangles[i]) areas[i] = RC_NULL_AREA;

	if (!rcCreateHeightfield(&context, *field, config.width, config.height,
		config.bmin, config.bmax, config.cs, config.ch)) return false;
	if (!rcRasterizeTriangles(&context, input.vertices.data(), vertexCount, input.indices.data(),
		areas.data(), triangleCount, *field, config.walkableClimb)) return false;

	// 段差・崖・頭上の空間はRecastの標準フィルターで判定する。
	rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *field);
	rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *field);
	if (!rcBuildCompactHeightfield(&context, config.walkableHeight, config.walkableClimb,
		*field, *compact)) return false;
	field.reset();

	// コライダーの範囲を歩行不可にし、その後でキャラクター半径分を縮小する。
	for (const Bounds& obstacle : obstacles)
	{
		Point boundsMin = obstacle.min;
		boundsMin[1] -= settings.agentHeight;
		rcMarkBoxArea(&context, boundsMin.data(), obstacle.max.data(), RC_NULL_AREA, *compact);
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
	params.tileX = region.tileX;
	params.tileY = region.tileZ;
	rcVcopy(params.bmin, mesh->bmin);
	rcVcopy(params.bmax, mesh->bmax);
	params.walkableHeight = settings.agentHeight;
	params.walkableRadius = settings.agentRadius;
	params.walkableClimb = settings.agentMaxClimb;
	params.cs = config.cs;
	params.ch = config.ch;
	params.buildBvTree = true;

	unsigned char* data = nullptr;
	int dataSize = 0;
	if (mesh->npolys && !dtCreateNavMeshData(&params, &data, &dataSize)) return false;
	result.data.reset(data);
	result.size = dataSize;
	return true;
}
