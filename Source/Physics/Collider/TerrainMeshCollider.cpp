#include "Physics/Collider/TerrainMeshCollider.h"
#include "Rendering/Core/RenderContext.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Resource/ResourceManager.h"
#include "Gameplay/Actor/Actor.h"

#include <cmath>
#include <unordered_map>

TerrainMeshCollider::TerrainMeshCollider(
    Object* owner,
    LayerId layerId,
    Rigidbody* rigidbody,
    const CollisionArea& collisionArea,
    PxMaterial* material,
    bool rebuildOnAwake)
    : PhysicsComponent(owner, layerId),
      rigidbody(rigidbody),
      material(material),
      collisionArea(collisionArea),
      rebuildOnAwake(rebuildOnAwake)
{
    _ASSERT_EXPR(rigidbody != nullptr, L"TerrainMeshCollider requires Rigidbody.");

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();

    std::vector<Vector3> vertices;
    std::vector<uint32_t> indices;
    if (LoadCachedMesh(vertices, indices))
    {
        debugVertices = vertices;
        debugIndices = indices;
    }
    else
    {
        pendingGpuRebuild = true;
    }
}

void TerrainMeshCollider::OnAwake()
{
    if (!debugVertices.empty() && !debugIndices.empty())
        BuildChunksFromCachedMesh(debugVertices, debugIndices);
    else if (pendingGpuRebuild && rebuildOnAwake)
        RebuildFromTerrain();
}

TerrainMeshCollider::~TerrainMeshCollider()
{
    for (ColliderChunk& chunk : chunks) ReleaseShape(chunk.shape);
}

void TerrainMeshCollider::ReleaseShape(PxShape*& shape)
{
    if (!shape) return;

    if (rigidbody)
    {
        if (PxRigidActor* rigidActor = rigidbody->GetRigidActor())
        {
            rigidActor->detachShape(*shape);
        }
    }

    shape->release();
    shape = nullptr;
}

// 動的地形コライダーのチャンク分割

void TerrainMeshCollider::InitializeChunks()
{
    if (chunks.size() == ChunkCountPerAxis * ChunkCountPerAxis &&
        fabsf(chunks.front().area.minX - collisionArea.minX) <= eps &&
        fabsf(chunks.front().area.minZ - collisionArea.minZ) <= eps &&
        fabsf(chunks.back().area.maxX - collisionArea.maxX) <= eps &&
        fabsf(chunks.back().area.maxZ - collisionArea.maxZ) <= eps)
    {
        return;
    }

    for (ColliderChunk& chunk : chunks) ReleaseShape(chunk.shape);
    chunks.clear();
    chunks.reserve(ChunkCountPerAxis * ChunkCountPerAxis);

    const float rangeX = collisionArea.maxX - collisionArea.minX;
    const float rangeZ = collisionArea.maxZ - collisionArea.minZ;
    for (int z = 0; z < ChunkCountPerAxis; ++z)
    {
        for (int x = 0; x < ChunkCountPerAxis; ++x)
        {
            ColliderChunk chunk;
            chunk.area.minX = collisionArea.minX + rangeX * x / ChunkCountPerAxis;
            chunk.area.maxX = collisionArea.minX + rangeX * (x + 1) / ChunkCountPerAxis;
            chunk.area.minZ = collisionArea.minZ + rangeZ * z / ChunkCountPerAxis;
            chunk.area.maxZ = collisionArea.minZ + rangeZ * (z + 1) / ChunkCountPerAxis;
            chunks.push_back(std::move(chunk));
        }
    }
}

void TerrainMeshCollider::BuildChunksFromCachedMesh(
    const std::vector<Vector3>& vertices,
    const std::vector<uint32_t>& indices)
{
    InitializeChunks();
    for (ColliderChunk& chunk : chunks)
    {
        chunk.vertices.clear();
        chunk.indices.clear();
    }

    Terrain* terrain = owner->GetComponent<Terrain>();
    Transform* transform = owner->GetComponent<Transform>();
    if (!terrain || !transform) return;

    const float width = terrain->GetTerrainSize() * transform->scale.x;
    const float depth = terrain->GetTerrainSize() * transform->scale.z;
    if (fabsf(width) <= eps || fabsf(depth) <= eps) return;

    std::vector<std::unordered_map<uint32_t, uint32_t>> vertexMaps(chunks.size());
    for (size_t index = 0; index + 2 < indices.size(); index += 3)
    {
        const uint32_t sourceIndices[] = {indices[index], indices[index + 1], indices[index + 2]};
        const Vector3 center =
            (vertices[sourceIndices[0]] + vertices[sourceIndices[1]] + vertices[sourceIndices[2]]) /
            3.0f;
        const float u = center.x / width + 0.5f;
        const float v = center.z / depth + 0.5f;
        const float rangeX = collisionArea.maxX - collisionArea.minX;
        const float rangeZ = collisionArea.maxZ - collisionArea.minZ;
        const int chunkX = std::clamp(static_cast<int>(
            (u - collisionArea.minX) / rangeX * ChunkCountPerAxis), 0, ChunkCountPerAxis - 1);
        const int chunkZ = std::clamp(static_cast<int>(
            (v - collisionArea.minZ) / rangeZ * ChunkCountPerAxis), 0, ChunkCountPerAxis - 1);
        const int chunkIndex = chunkZ * ChunkCountPerAxis + chunkX;
        ColliderChunk& chunk = chunks[chunkIndex];
        auto& vertexMap = vertexMaps[chunkIndex];

        for (uint32_t sourceIndex : sourceIndices)
        {
            auto [it, inserted] = vertexMap.emplace(
                sourceIndex, static_cast<uint32_t>(chunk.vertices.size()));
            if (inserted) chunk.vertices.push_back(vertices[sourceIndex]);
            chunk.indices.push_back(it->second);
        }
    }

    for (ColliderChunk& chunk : chunks)
    {
        UpdateShape(chunk.shape, chunk.vertices, chunk.indices);
    }
    pendingGpuRebuild = false;
}

void TerrainMeshCollider::RebuildChunk(ColliderChunk& chunk)
{
    Terrain* terrain = owner->GetComponent<Terrain>();
    if (!terrain) return;

    if (!terrain->BuildGpuColliderMesh(
        chunk.area.minX,
        chunk.area.maxX,
        chunk.area.minZ,
        chunk.area.maxZ,
        chunk.vertices,
        chunk.indices))
    {
        return;
    }

    ApplyOwnerScale(chunk.vertices);
    UpdateShape(chunk.shape, chunk.vertices, chunk.indices);
}

void TerrainMeshCollider::RefreshDebugMesh()
{
    debugVertices.clear();
    debugIndices.clear();
    for (const ColliderChunk& chunk : chunks)
    {
        const uint32_t vertexOffset = static_cast<uint32_t>(debugVertices.size());
        debugVertices.insert(debugVertices.end(), chunk.vertices.begin(), chunk.vertices.end());
        for (uint32_t index : chunk.indices) debugIndices.push_back(vertexOffset + index);
    }
}

void TerrainMeshCollider::ClampCollisionArea()
{
    collisionArea.minX = std::clamp(collisionArea.minX, 0.0f, 1.0f);
    collisionArea.minZ = std::clamp(collisionArea.minZ, 0.0f, 1.0f);
    collisionArea.maxX = std::clamp(collisionArea.maxX, 0.0f, 1.0f);
    collisionArea.maxZ = std::clamp(collisionArea.maxZ, 0.0f, 1.0f);

    if (collisionArea.minX > collisionArea.maxX)
    {
        std::swap(collisionArea.minX, collisionArea.maxX);
    }

    if (collisionArea.minZ > collisionArea.maxZ)
    {
        std::swap(collisionArea.minZ, collisionArea.maxZ);
    }

    constexpr float minRange = 0.001f;
    if (collisionArea.maxX - collisionArea.minX < minRange)
    {
        collisionArea.maxX = std::min(collisionArea.minX + minRange, 1.0f);
        collisionArea.minX = std::max(collisionArea.maxX - minRange, 0.0f);
    }

    if (collisionArea.maxZ - collisionArea.minZ < minRange)
    {
        collisionArea.maxZ = std::min(collisionArea.minZ + minRange, 1.0f);
        collisionArea.minZ = std::max(collisionArea.maxZ - minRange, 0.0f);
    }
}

void TerrainMeshCollider::RebuildFromTerrain(bool saveCache)
{
    Terrain* terrain = owner->GetComponent<Terrain>();
    _ASSERT_EXPR(terrain != nullptr, L"TerrainMeshCollider requires Terrain component.");

    ClampCollisionArea();
    InitializeChunks();
    for (ColliderChunk& chunk : chunks) RebuildChunk(chunk);
    RefreshDebugMesh();
    if (saveCache) SaveCachedMesh(debugVertices, debugIndices);
    pendingGpuRebuild = false;
}

void TerrainMeshCollider::RebuildRegionFromTerrain(
    float minX,
    float maxX,
    float minZ,
    float maxZ)
{
    ClampCollisionArea();
    InitializeChunks();
    minX = std::clamp(minX, collisionArea.minX, collisionArea.maxX);
    maxX = std::clamp(maxX, collisionArea.minX, collisionArea.maxX);
    minZ = std::clamp(minZ, collisionArea.minZ, collisionArea.maxZ);
    maxZ = std::clamp(maxZ, collisionArea.minZ, collisionArea.maxZ);
    if (minX > maxX) std::swap(minX, maxX);
    if (minZ > maxZ) std::swap(minZ, maxZ);

    for (ColliderChunk& chunk : chunks)
    {
        if (chunk.area.maxX < minX || chunk.area.minX > maxX ||
            chunk.area.maxZ < minZ || chunk.area.minZ > maxZ)
        {
            continue;
        }
        RebuildChunk(chunk);
    }

    RefreshDebugMesh();
    pendingGpuRebuild = false;
}

bool TerrainMeshCollider::LoadCachedMesh(
    std::vector<Vector3>& vertices,
    std::vector<uint32_t>& indices)
{
	const auto nearlyEqual = [](float a, float b)
	{
		return std::fabs(a - b) <= 0.0001f;
	};

    Terrain* terrain = owner->GetComponent<Terrain>();
    if (!terrain)
    {
        vxMessage = "Terrain .vx load skipped: Terrain is missing.";
        return false;
    }

    ClampCollisionArea();
    const std::filesystem::path filepath = terrain->GetColliderVertexPath();
    if (filepath.empty())
    {
        vxMessage = "Embedded terrain collider will be rebuilt in memory.";
        return false;
    }
    if (!std::filesystem::exists(filepath))
    {
        vxMessage = "Terrain .vx not found. GPU bake will create it.";
        return false;
    }

    std::ifstream file(filepath, std::ios::binary);
    if (!file)
    {
        vxMessage = "Terrain .vx load failed: open failed.";
        return false;
    }

    TerrainColliderVxHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (!file ||
        header.magic[0] != 'V' ||
        header.magic[1] != 'V' ||
        header.magic[2] != 'V' ||
        header.magic[3] != 'X' ||
        header.version != 3 ||
        header.vertexCount == 0 ||
        header.indexCount == 0)
    {
        vxMessage = "Terrain .vx load failed: invalid header.";
        return false;
    }

    Transform* transform = owner->GetComponent<Transform>();
    if (!transform) return false;
    const Vector3 scale = transform->scale;
    const uint64_t terrainDataHash = terrain->GetTerrainDataHash();
    if (!nearlyEqual(header.minX, collisionArea.minX) ||
        !nearlyEqual(header.maxX, collisionArea.maxX) ||
        !nearlyEqual(header.minZ, collisionArea.minZ) ||
        !nearlyEqual(header.maxZ, collisionArea.maxZ) ||
        header.terrainDataHash != terrainDataHash ||
        !nearlyEqual(header.scaleX, scale.x) ||
        !nearlyEqual(header.scaleY, scale.y) ||
        !nearlyEqual(header.scaleZ, scale.z) ||
        !nearlyEqual(header.terrainSize, terrain->GetTerrainSize()) ||
        header.gridResolution != terrain->GetGridResolution() ||
        !nearlyEqual(header.edgeFactor, terrain->GetTessellationEdgeFactor()) ||
        !nearlyEqual(header.innerFactor, terrain->GetTessellationInnerFactor()) ||
        !nearlyEqual(header.heightScaler, terrain->GetHeightScaler()))
    {
        vxMessage = "Terrain .vx ignored: terrain collider setting changed.";
        return false;
    }

    vertices.resize(header.vertexCount);
    indices.resize(header.indexCount);

    file.read(
        reinterpret_cast<char*>(vertices.data()),
        sizeof(Vector3) * vertices.size());
    file.read(
        reinterpret_cast<char*>(indices.data()),
        sizeof(uint32_t) * indices.size());

    if (!file)
    {
        vertices.clear();
        indices.clear();
        vxMessage = "Terrain .vx load failed: data is truncated.";
        return false;
    }

    vxMessage = "Terrain .vx loaded: " + filepath.generic_string();
    return true;
}

bool TerrainMeshCollider::SaveCachedMesh(
    const std::vector<Vector3>& vertices,
    const std::vector<uint32_t>& indices)
{
    Terrain* terrain = owner->GetComponent<Terrain>();
    if (!terrain || vertices.empty() || indices.empty())
    {
        vxMessage = "Terrain .vx save skipped: mesh is empty.";
        return false;
    }

    const std::filesystem::path filepath = terrain->GetColliderVertexPath();
    if (filepath.empty())
    {
        vxMessage = "Embedded terrain collider is kept in memory.";
        return false;
    }
    std::error_code error;
    if (!filepath.parent_path().empty())
    {
        std::filesystem::create_directories(filepath.parent_path(), error);
    }

    if (error)
    {
        vxMessage = "Terrain .vx save failed: directory creation failed.";
        return false;
    }

    std::ofstream file(filepath, std::ios::binary);
    if (!file)
    {
        vxMessage = "Terrain .vx save failed: open failed.";
        return false;
    }

    Transform* transform = owner->GetComponent<Transform>();
    if (!transform) return false;
    const Vector3 scale = transform->scale;

    TerrainColliderVxHeader header{};
    header.vertexCount = static_cast<uint32_t>(vertices.size());
    header.indexCount = static_cast<uint32_t>(indices.size());
    header.terrainDataHash = terrain->GetTerrainDataHash();
    header.minX = collisionArea.minX;
    header.maxX = collisionArea.maxX;
    header.minZ = collisionArea.minZ;
    header.maxZ = collisionArea.maxZ;
    header.scaleX = scale.x;
    header.scaleY = scale.y;
    header.scaleZ = scale.z;
    header.terrainSize = terrain->GetTerrainSize();
    header.gridResolution = terrain->GetGridResolution();
    header.edgeFactor = terrain->GetTessellationEdgeFactor();
    header.innerFactor = terrain->GetTessellationInnerFactor();
    header.heightScaler = terrain->GetHeightScaler();

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(
        reinterpret_cast<const char*>(vertices.data()),
        sizeof(Vector3) * vertices.size());
    file.write(
        reinterpret_cast<const char*>(indices.data()),
        sizeof(uint32_t) * indices.size());

    if (!file)
    {
        vxMessage = "Terrain .vx save failed: write failed.";
        return false;
    }

	vxMessage = "Terrain .vx saved: " + filepath.generic_string();
	ResourceManager::Instance().RegisterGeneratedCache(filepath.generic_string());
	return true;
}

void TerrainMeshCollider::ApplyOwnerScale(std::vector<Vector3>& vertices) const
{
    Transform* transform = owner->GetComponent<Transform>();
    if (!transform) return;
    const Vector3 scale = transform->scale;
    for (Vector3& vertex : vertices)
    {
        vertex.x *= scale.x;
        vertex.y *= scale.y;
        vertex.z *= scale.z;
    }
}

void TerrainMeshCollider::UpdateShape(
    PxShape*& shape,
    const std::vector<Vector3>& vertices,
    const std::vector<uint32_t>& indices)
{
    if (vertices.empty() || indices.empty())
    {
        ReleaseShape(shape);
        return;
    }

    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
    PxCookingParams* cookingParams = PhysicsManager::Instance().GetCooking();
    PxRigidActor* rigidActor = rigidbody->GetRigidActor();

    _ASSERT_EXPR(physics != nullptr, L"PhysX is not initialized.");
    _ASSERT_EXPR(cookingParams != nullptr, L"PhysX cooking is not initialized.");
    _ASSERT_EXPR(rigidActor != nullptr, L"TerrainMeshCollider Rigidbody has no PhysX actor.");
    _ASSERT_EXPR(rigidActor->is<PxRigidStatic>() != nullptr, L"TerrainMeshCollider requires RigidbodyStatic.");

    ReleaseShape(shape);

    std::vector<PxVec3> pxVertices;
    pxVertices.reserve(vertices.size());

    for (const Vector3& vertex : vertices)
    {
        pxVertices.emplace_back(vertex.x, vertex.y, vertex.z);
    }

    PxTriangleMeshDesc meshDesc{};
    meshDesc.points.count = static_cast<PxU32>(pxVertices.size());
    meshDesc.points.stride = sizeof(PxVec3);
    meshDesc.points.data = pxVertices.data();

    meshDesc.triangles.count = static_cast<PxU32>(indices.size() / 3);
    meshDesc.triangles.stride = 3 * sizeof(uint32_t);
    meshDesc.triangles.data = indices.data();

    PxDefaultMemoryOutputStream writeBuffer;
    const bool cooked = PxCookTriangleMesh(*cookingParams, meshDesc, writeBuffer);
    _ASSERT_EXPR(cooked, L"Failed to cook Terrain TriangleMesh.");

    if (!cooked)
    {
        return;
    }

    PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
    PxTriangleMesh* triangleMesh = physics->createTriangleMesh(readBuffer);
    _ASSERT_EXPR(triangleMesh != nullptr, L"Failed to create Terrain TriangleMesh.");

    if (triangleMesh == nullptr)
    {
        return;
    }

    shape = physics->createShape(PxTriangleMeshGeometry(triangleMesh), *material);
    triangleMesh->release();

    _ASSERT_EXPR(shape != nullptr, L"Failed to create TerrainMeshCollider shape.");

    if (shape == nullptr)
    {
        return;
    }

    shape->userData = this;

    PhysicsManager::SetLayerToShape(shape, layerId);
    rigidActor->attachShape(*shape);
}

void TerrainMeshCollider::Render(const RenderContext& rc)
{
    if (!showDebug) return;

    Terrain* terrain = owner->GetComponent<Terrain>();
    if (terrain != nullptr)
    {
        const float terrainSize = terrain->GetTerrainSize();
        const float halfTerrainSize = terrainSize * 0.5f;

        // collision range
        Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
            Vector3(
            (collisionArea.minX + collisionArea.maxX - 1.0f) * 0.5f * halfTerrainSize,
            0.0f,
            (collisionArea.minZ + collisionArea.maxZ - 1.0f) * 0.5f * halfTerrainSize),
            Vector3::Zero,
            Vector3(
            (collisionArea.maxX - collisionArea.minX) * halfTerrainSize,
            0.1f,
            (collisionArea.maxZ - collisionArea.minZ) * halfTerrainSize),
            Color(0.0f, 1.0f, 0.0f, 1.0f));
    }

}

void TerrainMeshCollider::DrawGUI()
{
    Terrain* terrain = owner->GetComponent<Terrain>();
    if (terrain != nullptr && ImGui::TreeNode("Collision Area AABB"))
    {
        ClampCollisionArea();

        ImGui::DragFloat("Min X", &collisionArea.minX, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Max X", &collisionArea.maxX, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Min Z", &collisionArea.minZ, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Max Z", &collisionArea.maxZ, 0.01f, 0.0f, 1.0f);

        if (ImGui::Button("Reset Full Terrain Area"))
        {
            collisionArea.minX = 0.0f;
            collisionArea.minZ = 0.0f;
            collisionArea.maxX = 1.0f;
            collisionArea.maxZ = 1.0f;
        }

        ImGui::TreePop();
    }

    if (ImGui::Button("Remake Terrain .vx Collider"))
    {
        RequestGpuRebuild();
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Terrain .vx Collider"))
    {
        std::vector<Vector3> vertices;
        std::vector<uint32_t> indices;
        if (LoadCachedMesh(vertices, indices))
        {
            BuildChunksFromCachedMesh(vertices, indices);
            debugVertices = vertices;
            debugIndices = indices;
        }
    }

    if (ImGui::Button("Save Current Terrain .vx Collider"))
    {
        SaveCachedMesh(debugVertices, debugIndices);
    }

    if (terrain != nullptr)
    {
        ImGui::Text("HeightMap: %d x %d",
            terrain->GetHeightMapWidth(),
            terrain->GetHeightMapHeight());
        ImGui::Text("VX: %s", terrain->GetColliderVertexPath().generic_string().c_str());
    }
    ImGui::Text("%s", vxMessage.c_str());
    ImGui::Text("Vertices: %d", static_cast<int>(debugVertices.size()));
    ImGui::Text("Triangles: %d", static_cast<int>(debugIndices.size() / 3));

    if (ImGui::TreeNode(ICON_FA_GRIP_LINES "Material"))
    {
        float staticFriction = material->getStaticFriction();
        float dynamicFriction = material->getDynamicFriction();
        float restitution = material->getRestitution();

        if (ImGui::DragFloat("Static Friction", &staticFriction, 0.01f, 0.0f, 1.0f))
        {
            material->setStaticFriction(staticFriction);
        }

        if (ImGui::DragFloat("Dynamic Friction", &dynamicFriction, 0.01f, 0.0f, 1.0f))
        {
            material->setDynamicFriction(dynamicFriction);
        }

        if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f))
        {
            material->setRestitution(restitution);
        }

        ImGui::TreePop();
    }
}
