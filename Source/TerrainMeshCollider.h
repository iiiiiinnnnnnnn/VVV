// TerrainMeshCollider.h

#pragma once

#include "CollidersDef.h"

class TerrainMeshCollider : public PhysicsComponent
{
public:
    struct CollisionArea
    {
        float minX = 0.0f, maxX = 1.0f, minZ = 0.0f, maxZ = 1.0f;
    };

    TerrainMeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, const CollisionArea& collisionArea = {}, PxMaterial* material = nullptr);
    ~TerrainMeshCollider() override;

    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    void RebuildFromTerrain();
    bool NeedsGpuRebuild() const { return pendingGpuRebuild; }
    void RequestGpuRebuild() { pendingGpuRebuild = true; }

    struct TerrainColliderVxHeader
    {
        char magic[4] = {'V', 'V', 'V', 'X'};
        uint32_t version = 1;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        float minX = 0.0f;
        float maxX = 1.0f;
        float minZ = 0.0f;
        float maxZ = 1.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float scaleZ = 1.0f;
        float terrainSize = 500.0f;
        int gridResolution = 0;
        float edgeFactor = 0.0f;
        float innerFactor = 0.0f;
        float heightScaler = 0.0f;
    };

private:
    bool LoadCachedMesh(std::vector<Vector3>& vertices, std::vector<uint32_t>& indices);
    bool SaveCachedMesh(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices);
    void ApplyOwnerScale(std::vector<Vector3>& vertices) const;
    void UpdateShape(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices);
    void ReleaseShape();
    void ClampCollisionArea();

    bool NearlyEqual(float a, float b)
    {
        return std::fabs(a - b) <= 0.0001f;
    }

    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;

    CollisionArea collisionArea;

    std::vector<Vector3> debugVertices;
    std::vector<uint32_t> debugIndices;
    std::string vxMessage;
    bool pendingGpuRebuild = false;
};