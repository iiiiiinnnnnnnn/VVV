#include "NavMeshBuilder.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Recast.h"
#include "DetourAlloc.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"

// ボーダーを除いたセル数を数え、各段階を同じ範囲で比較する。
static int CountWalkableCells(const rcHeightfield& field, int border)
{
	int count = 0;
	for (int z = border; z < field.height - border; ++z)
		for (int x = border; x < field.width - border; ++x)
			for (const rcSpan* span = field.spans[z * field.width + x]; span; span = span->next)
				if (span->area != RC_NULL_AREA)
				{
					++count;
					break;
				}
	return count;
}

static int CountWalkableCells(const rcCompactHeightfield& field, int border, bool requireRegion = false)
{
	int count = 0;
	for (int z = border; z < field.height - border; ++z)
	{
		for (int x = border; x < field.width - border; ++x)
		{
			const rcCompactCell& cell = field.cells[z * field.width + x];
			for (unsigned int i = cell.index; i < cell.index + cell.count; ++i)
				if (field.areas[i] != RC_NULL_AREA && (!requireRegion ||
					(field.spans[i].reg && !(field.spans[i].reg & RC_BORDER_REG))))
				{
					++count;
					break;
				}
		}
	}
	return count;
}

void NavMeshBuilder::DataDeleter::operator()(unsigned char* data) const
{
	dtFree(data);
}

int NavMeshBuilder::GetBorderSize()
{
	// 輪郭生成に必要な周辺セル。通行領域を縮小する半径ではない。
	return 3;
}

NavMeshBuilder::BuildRegion NavMeshBuilder::GetTileRegion(const Tile& tile, const BuildSettings& settings)
{
	BuildRegion region;
	region.tileX = tile.x;
	region.tileZ = tile.z;
	region.borderSize = GetBorderSize();
	region.bounds.min = {
		tile.origin[0] + (tile.x * tile.cellCount - region.borderSize) * settings.cellSize,
		tile.minY,
		tile.origin[2] + (tile.z * tile.cellCount - region.borderSize) * settings.cellSize};
	const float extent = (tile.cellCount + region.borderSize * 2) * settings.cellSize;
	region.bounds.max = {region.bounds.min[0] + extent, tile.maxY, region.bounds.min[2] + extent};
	return region;
}

bool NavMeshBuilder::BuildTile(const HeightMap& heightMap, const BuildSettings& settings,
	const Tile& tile, TileData& result)
{
	if (settings.cellSize <= 0.0f || tile.cellCount <= 0)
	{
		result = {};
		return false;
	}
	return BuildFromHeightMap(heightMap, settings, GetTileRegion(tile, settings), result);
}

bool NavMeshBuilder::BuildFromHeightMap(const HeightMap& heightMap, const BuildSettings& settings,
	const BuildRegion& region, TileData& result)
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
	CellMask cellMask;
	cellMask.values = heightMap.walkableMask;
	cellMask.width = heightMap.width - 1;
	cellMask.height = heightMap.height - 1;
	cellMask.originX = heightMap.originX;
	cellMask.originZ = heightMap.originZ;
	cellMask.spacingX = heightMap.spacingX;
	cellMask.spacingZ = heightMap.spacingZ;
	return BuildFromMesh({vertices, indices, masks, cellMask}, settings, region, result);
}

bool NavMeshBuilder::BuildFromMesh(const Mesh& input, const BuildSettings& settings,
	const BuildRegion& region, TileData& result)
{
	result = {};
	if (settings.cellSize <= 0.0f || settings.cellHeight <= 0.0f || region.borderSize < 0) return false;
	for (int axis = 0; axis < 3; ++axis)
		if (region.bounds.min[axis] >= region.bounds.max[axis]) return false;
	if (input.vertices.size() % 3 || input.indices.size() % 3 ||
		(!input.walkableTriangles.empty() && input.walkableTriangles.size() != input.indices.size() / 3))
		return false;
	for (int index : input.indices)
		if (index < 0 || static_cast<std::size_t>(index) >= input.vertices.size() / 3) return false;
	if (!input.cellMask.values.empty() && (input.cellMask.width <= 0 || input.cellMask.height <= 0 ||
		input.cellMask.spacingX <= 0.0f || input.cellMask.spacingZ <= 0.0f ||
		input.cellMask.values.size() != static_cast<std::size_t>(input.cellMask.width) * input.cellMask.height))
		return false;
	if (input.indices.empty()) return true;

	// 生成条件をRecastの単位へ変換する。タイル管理やゲーム設定はここへ持ち込まない。
	rcConfig config{};
	config.cs = settings.cellSize;
	config.ch = std::max(settings.cellHeight,
		(region.bounds.max[1] - region.bounds.min[1]) / (RC_SPAN_MAX_HEIGHT - 1));
	// 従来どおりAマスクだけで歩行範囲を決める。段差は全高域で接続する。
	config.walkableHeight = 0;
	config.walkableClimb = RC_SPAN_MAX_HEIGHT;
	config.walkableRadius = 0;
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

	BuildStats& stats = result.stats;
	stats.stage = "triangles";
	const int triangleCount = static_cast<int>(input.indices.size() / 3);
	const int vertexCount = static_cast<int>(input.vertices.size() / 3);
	stats.inputTriangles = triangleCount;
	std::vector<unsigned char> areas(triangleCount, RC_NULL_AREA);
	rcMarkWalkableTriangles(&context, config.walkableSlopeAngle,
		input.vertices.data(), vertexCount, input.indices.data(), triangleCount, areas.data());
	for (int i = 0; i < triangleCount; ++i)
	{
		const float* a = &input.vertices[input.indices[i * 3] * 3];
		const float* b = &input.vertices[input.indices[i * 3 + 1] * 3];
		const float* c = &input.vertices[input.indices[i * 3 + 2] * 3];
		const float normalY = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]);
		if (normalY > 0.0f) ++stats.upwardTriangles;
		if (!input.walkableTriangles.empty() && !input.walkableTriangles[i]) continue;
		areas[i] = RC_WALKABLE_AREA;
		++stats.walkableTriangles;
	}

	stats.stage = "heightfield";
	if (!rcCreateHeightfield(&context, *field, config.width, config.height,
		config.bmin, config.bmax, config.cs, config.ch)) return false;
	stats.stage = "rasterize";
	if (!rcRasterizeTriangles(&context, input.vertices.data(), vertexCount, input.indices.data(),
		areas.data(), triangleCount, *field, config.walkableClimb)) return false;

	stats.rasterizedCells = CountWalkableCells(*field, config.borderSize);

	// ラスタライズ時の境界へのにじみで、赤いマスクのセルが復活しないようにする。
	const CellMask& mask = input.cellMask;
	if (!mask.values.empty())
	{
		for (int z = 0; z < field->height; ++z)
		{
			for (int x = 0; x < field->width; ++x)
			{
				const float worldX = field->bmin[0] + (x + 0.5f) * field->cs;
				const float worldZ = field->bmin[2] + (z + 0.5f) * field->cs;
				const int mx = static_cast<int>(std::floor((worldX - mask.originX) / mask.spacingX));
				const int mz = static_cast<int>(std::floor((worldZ - mask.originZ) / mask.spacingZ));
				if (mx >= 0 && mx < mask.width && mz >= 0 && mz < mask.height &&
					mask.values[static_cast<std::size_t>(mz) * mask.width + mx]) continue;
				for (rcSpan* span = field->spans[z * field->width + x]; span; span = span->next)
					span->area = RC_NULL_AREA;
			}
		}
	}
	stats.maskedCells = CountWalkableCells(*field, config.borderSize);
	if (!stats.maskedCells)
	{
		stats.stage = "complete";
		return true;
	}

	// 傾斜・段差・頭上空間・半径・AABBによる追加の削除は行わない。
	stats.stage = "compact";
	if (!rcBuildCompactHeightfield(&context, config.walkableHeight, config.walkableClimb,
		*field, *compact)) return false;
	// ハイトマップには天井がない。Recastの8bit天井空間による暗黙の段差制限も外す。
	// ポリゴンの隣接生成はRecastへ任せ、単層の入力セル間だけを通行条件に合わせる。
	if (!input.cellMask.values.empty())
	{
		for (int z = 0; z < compact->height; ++z)
		{
			for (int x = 0; x < compact->width; ++x)
			{
				const rcCompactCell& cell = compact->cells[z * compact->width + x];
				if (cell.count != 1) continue;
				for (int direction = 0; direction < 4; ++direction)
	{
					const int nx = x + rcGetDirOffsetX(direction);
					const int nz = z + rcGetDirOffsetY(direction);
					if (nx < 0 || nx >= compact->width || nz < 0 || nz >= compact->height) continue;
					const rcCompactCell& neighbor = compact->cells[nz * compact->width + nx];
					if (neighbor.count == 1) rcSetCon(compact->spans[cell.index], direction, 0);
				}
			}
		}
	}
	stats.compactCells = CountWalkableCells(*compact, config.borderSize);
	field.reset();
	stats.stage = "distance field";
	if (!rcBuildDistanceField(&context, *compact)) return false;
	stats.distanceFieldCells = CountWalkableCells(*compact, config.borderSize);
	stats.stage = "regions";
	if (!rcBuildRegions(&context, *compact, config.borderSize,
		config.minRegionArea, config.mergeRegionArea)) return false;
	stats.regionCells = CountWalkableCells(*compact, config.borderSize, true);
	stats.stage = "contours";
	if (!rcBuildContours(&context, *compact, config.maxSimplificationError,
		config.maxEdgeLen, *contours)) return false;
	stats.contourCells = CountWalkableCells(*compact, config.borderSize, true);
	stats.contours = contours->nconts;
	stats.stage = "polygon mesh";
	if (!rcBuildPolyMesh(&context, *contours, config.maxVertsPerPoly, *mesh)) return false;
	stats.meshCells = CountWalkableCells(*compact, config.borderSize, true);
	stats.polygons = mesh->npolys;
	stats.stage = "detail mesh";
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
	params.walkableHeight = 0.0f;
	params.walkableRadius = 0.0f;
	params.walkableClimb = region.bounds.max[1] - region.bounds.min[1];
	params.cs = config.cs;
	params.ch = config.ch;
	params.buildBvTree = true;

	stats.detailTriangles = detail->ntris;
	stats.stage = "Detour data";
	unsigned char* data = nullptr;
	int dataSize = 0;
	if (mesh->npolys && !dtCreateNavMeshData(&params, &data, &dataSize)) return false;
	result.data.reset(data);
	result.size = dataSize;
	stats.stage = "complete";
	return true;
}
