// NavMeshActor.cpp

#include "Physics/Navigation/NavMeshActor.h"

#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Rendering/Core/Graphics.h"
#include "Physics/Navigation/NavMeshObstacle.h"
#include "Rendering/Renderer/PrimitiveRenderer.h"
#include "Rendering/Core/RenderContext.h"
#include "Gameplay/Stage/Component/Terrain.h"

#include "DetourAlloc.h"
#include "DetourNavMeshBuilder.h"
#include "DetourStatus.h"

#include <map>

NavMeshActor* NavMeshActor::active = nullptr;

NavMeshActor::NavMeshActor(Object* owner)
	: Component(owner)
{
	active = this;
}

NavMeshActor::~NavMeshActor()
{
	if (active == this)
		active = nullptr;

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
}

void NavMeshActor::Update()
{
	if (!buildRequested)
		return;

	if (buildDelayFrames > 0)
	{
		--buildDelayFrames;
		return;
	}

	buildRequested = false;
	Build();
}

void NavMeshActor::RequestBuild(int delayFrames)
{
	buildRequested = true;
	buildDelayFrames = std::max(delayFrames, 0);
}

void NavMeshActor::SetAgentRadius(float value)
{
	value = std::max(value, 0.0f);
	if (fabsf(agentRadius - value) <= 0.001f) return;

	agentRadius = value;
	RequestBuild();
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
	const Vector3& center,
	const std::vector<ObstacleBounds>& obstacles,
	float cellHalfSize) const
{
	for (const ObstacleBounds& obstacle : obstacles)
	{
		const Vector3 halfSize = obstacle.size * 0.5f;
		const float inflate = cellHalfSize;

		if (center.x < obstacle.center.x - halfSize.x - inflate) continue;
		if (center.x > obstacle.center.x + halfSize.x + inflate) continue;
		if (center.z < obstacle.center.z - halfSize.z - inflate) continue;
		if (center.z > obstacle.center.z + halfSize.z + inflate) continue;

		return true;
	}

	return false;
}

void NavMeshActor::Build()
{
	Release();

	Actor* actor = dynamic_cast<Actor*>(owner);
	Terrain* terrain = actor ? actor->GetComponent<Terrain>() : nullptr;
	if (!terrain)
	{
		statusMessage = "NavMesh build skipped: Terrain not found.";
		return;
	}

	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);

	const int grid = std::max(resolution, 2);
	const int vertSide = grid + 1;
	const int nvp = 3;
	const float terrainSize = terrain->GetTerrainSize();
	const float cellSize = terrainSize / static_cast<float>(grid);
	const float cellHeight = 0.25f;
	const float halfTerrainSize = terrainSize * 0.5f;

	std::vector<unsigned short> verts;
	verts.resize(static_cast<size_t>(vertSide) * static_cast<size_t>(vertSide) * 3);

	for (int z = 0; z < vertSide; ++z)
	{
		for (int x = 0; x < vertSide; ++x)
		{
			const float u = static_cast<float>(x) / static_cast<float>(grid);
			const float v = static_cast<float>(z) / static_cast<float>(grid);
			const float height = terrain->GetHeightByUV(u, v);
			const int index = (z * vertSide + x) * 3;

			verts[index + 0] = static_cast<unsigned short>(x);
			verts[index + 1] = static_cast<unsigned short>(std::max(0, static_cast<int>((height - navMinY) / cellHeight)));
			verts[index + 2] = static_cast<unsigned short>(z);
		}
	}

	std::vector<unsigned short> polys;
	std::vector<unsigned short> flags;
	std::vector<unsigned char> areas;
	int blockedCellCount = 0;
	debugCells.clear();
	debugCells.reserve(static_cast<size_t>(grid) * static_cast<size_t>(grid) * 2);

	auto makeDebugVertex = [&](int x, int z)
	{
		const float u = static_cast<float>(x) / static_cast<float>(grid);
		const float v = static_cast<float>(z) / static_cast<float>(grid);
		return Vector3(
			(u - 0.5f) * terrainSize,
			terrain->GetHeightByUV(u, v) + 0.08f,
			(v - 0.5f) * terrainSize);
	};

	auto pushDebugTriangle = [&](const Vector3& a, const Vector3& b, const Vector3& c, bool walkable)
	{
		DebugCell debugCell{};
		debugCell.corners[0] = a;
		debugCell.corners[1] = b;
		debugCell.corners[2] = c;
		debugCell.walkable = walkable;
		debugCells.push_back(debugCell);
	};

	auto pushNavTriangle = [&](unsigned short a, unsigned short b, unsigned short c)
	{
		const int polyIndex = static_cast<int>(flags.size());
		polys.resize(polys.size() + nvp * 2, 0xffff);
		unsigned short* poly = &polys[static_cast<size_t>(polyIndex) * nvp * 2];
		poly[0] = a;
		poly[1] = b;
		poly[2] = c;
		flags.push_back(1);
		areas.push_back(0);
	};

	for (int z = 0; z < grid; ++z)
	{
		for (int x = 0; x < grid; ++x)
		{
			const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(grid);
			const float v = (static_cast<float>(z) + 0.5f) / static_cast<float>(grid);
			Vector3 center(
				(u - 0.5f) * terrainSize,
				terrain->GetHeightByUV(u, v),
				(v - 0.5f) * terrainSize);

			const unsigned short i0 = static_cast<unsigned short>(z * vertSide + x);
			const unsigned short i1 = static_cast<unsigned short>(z * vertSide + x + 1);
			const unsigned short i2 = static_cast<unsigned short>((z + 1) * vertSide + x);
			const unsigned short i3 = static_cast<unsigned short>((z + 1) * vertSide + x + 1);

			const Vector3 p0 = makeDebugVertex(x, z);
			const Vector3 p1 = makeDebugVertex(x + 1, z);
			const Vector3 p2 = makeDebugVertex(x, z + 1);
			const Vector3 p3 = makeDebugVertex(x + 1, z + 1);

			if (IsBlockedByObstacle(center, obstacles, cellSize * 0.5f))
			{
				++blockedCellCount;
				pushDebugTriangle(p0, p2, p1, false);
				pushDebugTriangle(p1, p2, p3, false);
				continue;
			}

			pushDebugTriangle(p0, p2, p1, true);
			pushDebugTriangle(p1, p2, p3, true);
			pushNavTriangle(i0, i2, i1);
			pushNavTriangle(i1, i2, i3);
		}
	}

	struct EdgeRef
	{
		unsigned short polyIndex = 0xffff;
		unsigned short edgeIndex = 0xffff;
	};

	std::map<std::pair<unsigned short, unsigned short>, EdgeRef> edgeRefs;
	for (int polyIndex = 0; polyIndex < static_cast<int>(flags.size()); ++polyIndex)
	{
		unsigned short* poly = &polys[static_cast<size_t>(polyIndex) * nvp * 2];
		for (int edgeIndex = 0; edgeIndex < nvp; ++edgeIndex)
		{
			const unsigned short a = poly[edgeIndex];
			const unsigned short b = poly[(edgeIndex + 1) % nvp];
			const std::pair<unsigned short, unsigned short> key(std::min(a, b), std::max(a, b));

			auto it = edgeRefs.find(key);
			if (it == edgeRefs.end())
			{
				edgeRefs[key] =
				{
					static_cast<unsigned short>(polyIndex),
					static_cast<unsigned short>(edgeIndex)
				};
				continue;
			}

			unsigned short* otherPoly =
				&polys[static_cast<size_t>(it->second.polyIndex) * nvp * 2];
			poly[nvp + edgeIndex] = it->second.polyIndex;
			otherPoly[nvp + it->second.edgeIndex] =
				static_cast<unsigned short>(polyIndex);
		}
	}

	if (flags.empty())
	{
		statusMessage = "NavMesh build failed: no walkable polygons.";
		return;
	}

	dtNavMeshCreateParams params = {};
	params.verts = verts.data();
	params.vertCount = static_cast<int>(verts.size() / 3);
	params.polys = polys.data();
	params.polyFlags = flags.data();
	params.polyAreas = areas.data();
	params.polyCount = static_cast<int>(flags.size());
	params.nvp = nvp;
	params.bmin[0] = -halfTerrainSize;
	params.bmin[1] = navMinY;
	params.bmin[2] = -halfTerrainSize;
	params.bmax[0] = halfTerrainSize;
	params.bmax[1] = navMaxY;
	params.bmax[2] = halfTerrainSize;
	params.walkableHeight = agentHeight;
	params.walkableRadius = agentRadius;
	params.walkableClimb = agentClimb;
	params.cs = cellSize;
	params.ch = cellHeight;
	params.buildBvTree = true;

	unsigned char* data = nullptr;
	int dataSize = 0;
	if (!dtCreateNavMeshData(&params, &data, &dataSize))
	{
		statusMessage = "NavMesh build failed: dtCreateNavMeshData.";
		return;
	}

	navMesh = dtAllocNavMesh();
	if (!navMesh)
	{
		dtFree(data);
		statusMessage = "NavMesh build failed: dtAllocNavMesh.";
		return;
	}

	dtStatus status = navMesh->init(data, dataSize, DT_TILE_FREE_DATA);
	if (dtStatusFailed(status))
	{
		dtFree(data);
		Release();
		statusMessage = "NavMesh build failed: navMesh init.";
		return;
	}

	navQuery = dtAllocNavMeshQuery();
	if (!navQuery)
	{
		Release();
		statusMessage = "NavMesh build failed: dtAllocNavMeshQuery.";
		return;
	}

	status = navQuery->init(navMesh, 2048);
	if (dtStatusFailed(status))
	{
		Release();
		statusMessage = "NavMesh build failed: query init.";
		return;
	}

	built = true;
	statusMessage =
		"NavMesh built. polys=" + std::to_string(flags.size()) +
		" obstacles=" + std::to_string(obstacles.size()) +
		" blocked=" + std::to_string(blockedCellCount);
}

bool NavMeshActor::FindNextPoint(
	const Vector3& start,
	const Vector3& goal,
	Vector3& nextPoint) const
{
	if (!built || !navQuery)
		return false;

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

	if (dtStatusFailed(navQuery->findNearestPoly(startPos, halfExtents, &filter, &startRef, nearestStart)) || !startRef)
		return false;

	if (dtStatusFailed(navQuery->findNearestPoly(goalPos, halfExtents, &filter, &goalRef, nearestGoal)) || !goalRef)
		return false;

	dtPolyRef path[64] = {};
	int pathCount = 0;
	if (dtStatusFailed(navQuery->findPath(startRef, goalRef, nearestStart, nearestGoal, &filter, path, &pathCount, _countof(path))))
		return false;

	if (pathCount <= 0)
		return false;

	float straightPath[64 * 3] = {};
	unsigned char straightFlags[64] = {};
	dtPolyRef straightRefs[64] = {};
	int straightCount = 0;
	if (dtStatusFailed(navQuery->findStraightPath(
		nearestStart,
		nearestGoal,
		path,
		pathCount,
		straightPath,
		straightFlags,
		straightRefs,
		&straightCount,
		64)))
	{
		return false;
	}

	if (straightCount <= 0)
		return false;

	int pointIndex = straightCount - 1;
	const float minNextPointDistance = std::max(agentRadius * 0.1f, 0.05f);
	const float minNextPointDistanceSq = minNextPointDistance * minNextPointDistance;
	for (int index = 1; index < straightCount; ++index)
	{
		Vector3 delta(
			straightPath[index * 3 + 0] - start.x,
			0.0f,
			straightPath[index * 3 + 2] - start.z);
		if (delta.LengthSquared() <= minNextPointDistanceSq) continue;

		pointIndex = index;
		break;
	}

	nextPoint = Vector3(
		straightPath[pointIndex * 3 + 0],
		straightPath[pointIndex * 3 + 1],
		straightPath[pointIndex * 3 + 2]);
	return true;
}

bool NavMeshActor::IsDirectPathBlocked(const Vector3& start, const Vector3& goal) const
{
	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);

	const Vector3 delta = goal - start;
	for (const ObstacleBounds& obstacle : obstacles)
	{
		const Vector3 halfSize = obstacle.size * 0.5f;
		const float minX = obstacle.center.x - halfSize.x;
		const float maxX = obstacle.center.x + halfSize.x;
		const float minZ = obstacle.center.z - halfSize.z;
		const float maxZ = obstacle.center.z + halfSize.z;

		float enter = 0.0f;
		float exit = 1.0f;
		auto clipAxis = [&enter, &exit](float origin, float direction, float minValue, float maxValue)
		{
			if (fabsf(direction) <= eps)
				return origin >= minValue && origin <= maxValue;

			float first = (minValue - origin) / direction;
			float second = (maxValue - origin) / direction;
			if (first > second) std::swap(first, second);
			enter = std::max(enter, first);
			exit = std::min(exit, second);
			return enter <= exit;
		};

		if (clipAxis(start.x, delta.x, minX, maxX) &&
			clipAxis(start.z, delta.z, minZ, maxZ))
		{
			return true;
		}
	}

	return false;
}

bool NavMeshActor::FindObstacleDetourPoint(
	const Vector3& start,
	const Vector3& goal,
	Vector3& nextPoint) const
{
	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);

	auto intersects = [](const Vector3& from, const Vector3& to, const ObstacleBounds& obstacle)
	{
		const Vector3 halfSize = obstacle.size * 0.5f;
		const Vector3 delta = to - from;
		float enter = 0.0f;
		float exit = 1.0f;
		auto clipAxis = [&enter, &exit](float origin, float direction, float minValue, float maxValue)
		{
			if (fabsf(direction) <= eps) return origin >= minValue && origin <= maxValue;
			float first = (minValue - origin) / direction;
			float second = (maxValue - origin) / direction;
			if (first > second) std::swap(first, second);
			enter = std::max(enter, first);
			exit = std::min(exit, second);
			return enter <= exit;
		};

		return clipAxis(from.x, delta.x, obstacle.center.x - halfSize.x, obstacle.center.x + halfSize.x) &&
			clipAxis(from.z, delta.z, obstacle.center.z - halfSize.z, obstacle.center.z + halfSize.z);
	};

	float bestDistance = FLT_MAX;
	bool found = false;
	const float clearance = std::max(agentRadius, 0.1f);
	for (const ObstacleBounds& obstacle : obstacles)
	{
		if (!intersects(start, goal, obstacle)) continue;
		const Vector3 halfSize = obstacle.size * 0.5f;
		const Vector3 candidates[] =
		{
			Vector3(obstacle.center.x - halfSize.x - clearance, start.y, obstacle.center.z - halfSize.z - clearance),
			Vector3(obstacle.center.x + halfSize.x + clearance, start.y, obstacle.center.z - halfSize.z - clearance),
			Vector3(obstacle.center.x - halfSize.x - clearance, start.y, obstacle.center.z + halfSize.z + clearance),
			Vector3(obstacle.center.x + halfSize.x + clearance, start.y, obstacle.center.z + halfSize.z + clearance)
		};

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

			const float distance = Vector3::Distance(start, candidate) + Vector3::Distance(candidate, goal);
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
	if (!rc.renderSettings.showDebug ||
		!showNavMeshDebug ||
		!rc.renderSettings.showNavMeshDebug) return;

	PrimitiveRenderer* renderer =
		Game::Graphics::Instance().GetPrimitiveRenderer();
	if (!renderer) return;

	const int step = std::max(debugDrawStep, 1);
	const Color walkableColor(0.0f, 0.8f, 1.0f, 0.65f);
	const Color blockedColor(1.0f, 0.15f, 0.05f, 0.65f);

	for (int index = 0; index < static_cast<int>(debugCells.size()); ++index)
	{
		if (index % step != 0) continue;

		const DebugCell& cell = debugCells[index];
		if (cell.walkable && !showWalkableCells) continue;
		if (!cell.walkable && !showBlockedCells) continue;
		const Color color = cell.walkable ? walkableColor : blockedColor;
		renderer->DrawLine(cell.corners[0], cell.corners[1], color, color);
		renderer->DrawLine(cell.corners[1], cell.corners[2], color, color);
		renderer->DrawLine(cell.corners[2], cell.corners[0], color, color);
	}

	if (!showObstacleBounds) return;

	std::vector<ObstacleBounds> obstacles;
	CollectObstacles(obstacles);
	auto drawBounds = [renderer](
		const Vector3& center,
		const Vector3& size,
		const Color& color)
	{
		const Vector3 half = size * 0.5f;
		const Vector3 corners[] =
		{
			center + Vector3(-half.x, -half.y, -half.z),
			center + Vector3( half.x, -half.y, -half.z),
			center + Vector3( half.x, -half.y,  half.z),
			center + Vector3(-half.x, -half.y,  half.z),
			center + Vector3(-half.x,  half.y, -half.z),
			center + Vector3( half.x,  half.y, -half.z),
			center + Vector3( half.x,  half.y,  half.z),
			center + Vector3(-half.x,  half.y,  half.z)
		};
		constexpr int edges[][2] =
		{
			{0, 1}, {1, 2}, {2, 3}, {3, 0},
			{4, 5}, {5, 6}, {6, 7}, {7, 4},
			{0, 4}, {1, 5}, {2, 6}, {3, 7}
		};
		for (const auto& edge : edges)
			renderer->DrawLine(corners[edge[0]], corners[edge[1]], color, color);
	};

	const Color obstacleColor(1.0f, 1.0f, 1.0f, 1.0f);
	for (const ObstacleBounds& obstacle : obstacles)
		drawBounds(obstacle.center, obstacle.size, obstacleColor);
}

void NavMeshActor::DrawGUI()
{
	ImGui::Checkbox("Show NavMesh Debug", &showNavMeshDebug);
	if (showNavMeshDebug)
	{
		ImGui::Checkbox("Show Walkable Cells", &showWalkableCells);
		ImGui::Checkbox("Show Blocked Cells", &showBlockedCells);
		ImGui::Checkbox("Show MeshCollider Bounds", &showObstacleBounds);
		ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Cyan: walkable");
		ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.05f, 1.0f), "Red: blocked cell");
		ImGui::TextUnformatted("White: MeshCollider bounds");
	}

	if (ImGui::DragInt("Resolution", &resolution, 1.0f, 8, 256))
		resolution = std::max(resolution, 8);
	if (ImGui::DragInt("Debug Draw Step", &debugDrawStep, 1.0f, 1, 32))
		debugDrawStep = std::max(debugDrawStep, 1);
	ImGui::DragFloat("Agent Height", &agentHeight, 0.1f, 0.1f, 10.0f);
	ImGui::DragFloat("Agent Radius", &agentRadius, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat("Agent Climb", &agentClimb, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat("Nearest Poly Extent", &nearestPolyExtent, 0.1f, 0.1f, 50.0f);
	ImGui::Text("%s", statusMessage.c_str());

	if (ImGui::Button("Rebuild NavMesh"))
		buildRequested = true;
}
