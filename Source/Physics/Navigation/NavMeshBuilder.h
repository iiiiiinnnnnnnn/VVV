#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>

class NavMeshBuilder
{
public:
	using Point = std::array<float, 3>;

	struct Bounds
	{
		Point min = {};
		Point max = {};
	};

	struct BuildSettings
	{
		float cellSize = 0.5f;
		// 高さ範囲をRecastで表現できない場合は、この値より粗い刻みへ調整する。
		float cellHeight = 0.1f;
		float agentHeight = 2.0f;
		float agentRadius = 0.5f;
		float agentMaxClimb = 0.3f;
		float agentMaxSlope = 45.0f;
		int maxEdgeLength = 8;
		float contourMaxError = 1.3f;
		float detailSampleDistance = 3.0f;
		float detailMaxError = 0.02f;
		int minRegionArea = 0;
		int mergeRegionArea = 0;
	};

	struct HeightMap
	{
		// heightsは頂点単位の最終的なY座標。補間や高さ倍率は呼び出し側で適用する。
		std::span<const float> heights;
		// マスクはセル単位で0が通行不可。空なら全セルを入力に使用する。
		std::span<const std::uint8_t> walkableMask;
		int width = 0;
		int height = 0;
		float originX = 0.0f;
		float originZ = 0.0f;
		float spacingX = 1.0f;
		float spacingZ = 1.0f;
	};

	struct Mesh
	{
		// XYZの順に並べた頂点と、上向きの面になる三角形インデックス。
		std::span<const float> vertices;
		std::span<const int> indices;
		std::span<const std::uint8_t> walkableTriangles;
	};

	struct BuildRegion
	{
		// タイルでは周辺のボーダーを含む範囲を指定する。
		Bounds bounds;
		int tileX = 0;
		int tileZ = 0;
		int borderSize = 0;
	};

	struct Tile
	{
		Point origin = {};
		int x = 0;
		int z = 0;
		int cellCount = 32;
		float minY = -100.0f;
		float maxY = 100.0f;
	};

	struct DataDeleter
	{
		void operator()(unsigned char* data) const;
	};

	struct TileData
	{
		// Detourへ登録するまではこの結果が所有する。空の成功結果は通行領域なし。
		std::unique_ptr<unsigned char, DataDeleter> data;
		int size = 0;
	};

	static int GetBorderSize(const BuildSettings& settings);
	static BuildRegion GetTileRegion(const Tile& tile, const BuildSettings& settings);

	static bool BuildFromHeightMap(const HeightMap& heightMap, const BuildSettings& settings,
		const BuildRegion& region, std::span<const Bounds> obstacles, TileData& result);
	static bool BuildFromMesh(const Mesh& input, const BuildSettings& settings,
		const BuildRegion& region, std::span<const Bounds> obstacles, TileData& result);
	static bool BuildTile(const HeightMap& heightMap, const BuildSettings& settings,
		const Tile& tile, std::span<const Bounds> obstacles, TileData& result);
};
