// Collider.h

#pragma once

#include "Component.h"
#include "PhysicsManager.h"
#include "Object.h"
#include "Model.h"

class Rigidbody;

class BoxCollider : public Component {
public:
    BoxCollider(Object* owner, Rigidbody* rigidbody, const Vector3& size, PxMaterial* material = nullptr);
	void Render(const RenderContext& rc) override;
    void DrawGUI() override;

	const Vector3& GetSize() const { return size; }
private:
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
	Vector3 size;
};

class CapsuleCollider : public Component {
public:
    CapsuleCollider(Object* owner, Rigidbody* rigidbody, float radius, float height, PxMaterial* material = nullptr);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    const float GetRadius() const { return radius; }
    const float GetHeight() const { return height; }
private:
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    float radius;
    float height;
};

class SphereCollider : public Component {
public:
    SphereCollider(Object* owner, Rigidbody* rigidbody, float radius, PxMaterial* material = nullptr);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    const float GetRadius() const { return radius; }
private:
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    float radius;
};

class MeshCollider : public Component
{
public:
    // TriangleMesh（Static用）
    MeshCollider(Object* owner, Rigidbody* rigidbody, Model* model, PxMaterial* material = nullptr);
    // ConvexMesh（Dynamic用）
    MeshCollider(Object* owner, Rigidbody* rigidbody, Model* model, bool useConvex, unsigned int quantizedCount = 32, PxMaterial* material = nullptr);

    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
private:
    void UpdateShape();
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    Model* model = nullptr;
    bool useConvex = false;
    unsigned int quantizedCount = 32;
};

// ボーンを指定して追従　行列オフセットも設定できる。
class BoneSphereCollider : public Component
{
public:
    BoneSphereCollider(Object* owner, Model* model, int nodeIndex, float radius, Matrix offset = Matrix::Identity, PxMaterial* material = nullptr);
    ~BoneSphereCollider();
    void LateUpdate() override;         // 毎フレームボーン追従
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
private:
    void UpdateShape();
    PxShape* shape = nullptr;
    PxRigidDynamic* ghostActor = nullptr;  // Rigidbody*からPxRigidDynamic*に変更
    PxMaterial* material = nullptr;
    Model* model = nullptr;
    int nodeIndex = -1;
    Matrix offset;
    float radius = 0.5f;
};

class TerrainMeshCollider : public Component
{
public:
    static constexpr int MaxResolution = 2048;

    TerrainMeshCollider(Object* owner, Rigidbody* rigidbody, int resolution = 128, PxMaterial* material = nullptr);
    ~TerrainMeshCollider() override;

    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    void RebuildFromTerrain();

private:
    void BuildMeshFromTerrain(std::vector<Vector3>& vertices, std::vector<uint32_t>& indices);
    void UpdateShape(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices);
    void ReleaseShape();

private:
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;

    int resolution = 128;

    std::vector<Vector3> debugVertices;
    std::vector<uint32_t> debugIndices;
};
