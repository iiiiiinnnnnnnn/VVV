// TerrainMeshCollider.cpp

#include "TerrainMeshCollider.h"
#include "RenderContext.h"
#include "Rigidbody.h"
#include "Graphics.h"
#include "Terrain.h"
#include "Actor.h"

TerrainMeshCollider::TerrainMeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, const CollisionArea& collisionArea, PxMaterial* material)
    : PhysicsComponent(owner, layerId), rigidbody(rigidbody), collisionArea(collisionArea), material(material)
{
    GetOwnerAsActor();

    _ASSERT_EXPR(rigidbody != nullptr, L"TerrainMeshCollider requires Rigidbody.");

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();

    std::vector<Vector3> vertices;
    std::vector<uint32_t> indices;
    if (LoadCachedMesh(vertices, indices))
    {
        UpdateShape(vertices, indices);
        debugVertices = vertices;
        debugIndices = indices;
    }
    else
    {
        pendingGpuRebuild = true;
    }
}

TerrainMeshCollider::~TerrainMeshCollider()
{
    if (shape != nullptr)
    {
        shape->release();
        shape = nullptr;
    }
}

void TerrainMeshCollider::ReleaseShape()
{
    if (shape == nullptr)
    {
        return;
    }

    if (rigidbody != nullptr && rigidbody->GetRigidActor() != nullptr)
    {
        rigidbody->GetRigidActor()->detachShape(*shape);
    }

    shape->release();
    shape = nullptr;
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
        collisionArea.maxX = min(collisionArea.minX + minRange, 1.0f);
        collisionArea.minX = max(collisionArea.maxX - minRange, 0.0f);
    }

    if (collisionArea.maxZ - collisionArea.minZ < minRange)
    {
        collisionArea.maxZ = min(collisionArea.minZ + minRange, 1.0f);
        collisionArea.minZ = max(collisionArea.maxZ - minRange, 0.0f);
    }
}

void TerrainMeshCollider::RebuildFromTerrain()
{
    std::vector<Vector3> vertices;
    std::vector<uint32_t> indices;
    Terrain* terrain = owner->GetComponent<Terrain>();
    _ASSERT_EXPR(terrain != nullptr, L"TerrainMeshCollider requires Terrain component.");

    ClampCollisionArea();
    if (!terrain->BuildGpuColliderMesh(
        collisionArea.minX,
        collisionArea.maxX,
        collisionArea.minZ,
        collisionArea.maxZ,
        vertices,
        indices))
    {
        vxMessage = "GPU collider bake failed.";
        return;
    }

    ApplyOwnerScale(vertices);
    UpdateShape(vertices, indices);
    SaveCachedMesh(vertices, indices);

    debugVertices = vertices;
    debugIndices = indices;
    pendingGpuRebuild = false;
}

bool TerrainMeshCollider::LoadCachedMesh(
    std::vector<Vector3>& vertices,
    std::vector<uint32_t>& indices)
{
    Terrain* terrain = owner->GetComponent<Terrain>();
    if (!terrain)
    {
        vxMessage = "Terrain .vx load skipped: Terrain is missing.";
        return false;
    }

    ClampCollisionArea();
    const std::filesystem::path filepath = terrain->GetColliderVertexPath();
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

    Actor* actor = GetOwnerAsActor();
    const Vector3 scale = actor->transform.scale;
    const uint64_t terrainDataHash = terrain->GetTerrainDataHash();
    if (!NearlyEqual(header.minX, collisionArea.minX) ||
        !NearlyEqual(header.maxX, collisionArea.maxX) ||
        !NearlyEqual(header.minZ, collisionArea.minZ) ||
        !NearlyEqual(header.maxZ, collisionArea.maxZ) ||
        header.terrainDataHash != terrainDataHash ||
        !NearlyEqual(header.scaleX, scale.x) ||
        !NearlyEqual(header.scaleY, scale.y) ||
        !NearlyEqual(header.scaleZ, scale.z) ||
        !NearlyEqual(header.terrainSize, terrain->GetTerrainSize()) ||
        header.gridResolution != terrain->GetGridResolution() ||
        !NearlyEqual(header.edgeFactor, terrain->GetTessellationEdgeFactor()) ||
        !NearlyEqual(header.innerFactor, terrain->GetTessellationInnerFactor()) ||
        !NearlyEqual(header.heightScaler, terrain->GetHeightScaler()))
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

    Actor* actor = GetOwnerAsActor();
    const Vector3 scale = actor->transform.scale;

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
    return true;
}

void TerrainMeshCollider::ApplyOwnerScale(std::vector<Vector3>& vertices) const
{
    Actor* actor = GetOwnerAsActor();
    const Vector3 scale = actor->transform.scale;
    for (Vector3& vertex : vertices)
    {
        vertex.x *= scale.x;
        vertex.y *= scale.y;
        vertex.z *= scale.z;
    }
}

void TerrainMeshCollider::UpdateShape(
    const std::vector<Vector3>& vertices,
    const std::vector<uint32_t>& indices)
{
    if (vertices.empty() || indices.empty())
    {
        return;
    }

    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
    PxCookingParams* cookingParams = PhysicsManager::Instance().GetCooking();
    PxRigidActor* rigidActor = rigidbody->GetRigidActor();

    _ASSERT_EXPR(physics != nullptr, L"PhysX is not initialized.");
    _ASSERT_EXPR(cookingParams != nullptr, L"PhysX cooking is not initialized.");
    _ASSERT_EXPR(rigidActor != nullptr, L"TerrainMeshCollider Rigidbody has no PhysX actor.");
    _ASSERT_EXPR(rigidActor->is<PxRigidStatic>() != nullptr, L"TerrainMeshCollider requires RigidbodyStatic.");

    ReleaseShape();

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

    Actor* actor = GetOwnerAsActor();
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
            UpdateShape(vertices, indices);
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
