// Collider.h

#pragma once

#include "Component.h"
#include "PhysicsManager.h"
#include "Object.h"
#include "Model.h"

class Rigidbody;

class Collider : public Component
{
public:
    Collider(Object* owner) : Component(owner) {}
    Actor* GetOwnerActor() const { return GetOwnerAsActor(); }
};

class BoxCollider : public Collider {
public:
    BoxCollider(Object* owner, Rigidbody* rigidbody, const Vector3& size, PxMaterial* material = nullptr);
    BoxCollider(Object* owner, Rigidbody* rigidbody, const Vector3& size, const Vector3& localPosition, PxMaterial* material = nullptr);
	void Render(const RenderContext& rc) override;
    void DrawGUI() override;

	const Vector3& GetSize() const { return size; }
    const Vector3& GetLocalPosition() const { return localPosition; }
private:
    PxTransform MakeLocalPose() const;
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
	Vector3 size;
    Vector3 localPosition;
};

class CapsuleCollider : public Collider {
public:
    CapsuleCollider(Object* owner, Rigidbody* rigidbody, float radius, float height, PxMaterial* material = nullptr);
    CapsuleCollider(Object* owner, Rigidbody* rigidbody, float radius, float height, const Vector3& localPosition, PxMaterial* material = nullptr);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    const float GetRadius() const { return radius; }
    const float GetHeight() const { return height; }
    const Vector3& GetLocalPosition() const { return localPosition; }
private:
    PxTransform MakeLocalPose() const;
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    float radius;
    float height;
    Vector3 localPosition;
};

class SphereCollider : public Collider {
public:
    SphereCollider(Object* owner, Rigidbody* rigidbody, float radius, PxMaterial* material = nullptr);
    SphereCollider(Object* owner, Rigidbody* rigidbody, float radius, const Vector3& localPosition, PxMaterial* material = nullptr);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    const float GetRadius() const { return radius; }
    const Vector3& GetLocalPosition() const { return localPosition; }
private:
    PxTransform MakeLocalPose() const;
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    float radius;
    Vector3 localPosition;
};

class MeshCollider : public Collider
{
public:
    // TriangleMesh（Static用）
    MeshCollider(Object* owner, Rigidbody* rigidbody, Model* model, PxMaterial* material = nullptr);
    // ConvexMesh（Dynamic用）
    MeshCollider(Object* owner, Rigidbody* rigidbody, Model* model, bool useConvex, unsigned int quantizedCount = 32, PxMaterial* material = nullptr);

    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
    Vector3 GetWorldPosition() const;
private:
    void UpdateShape();
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    Model* model = nullptr;
    bool useConvex = false;
    unsigned int quantizedCount = 32;
};

class BoneCollider : public Collider
{
public:
    BoneCollider(Object* owner, Model* model, int nodeIndex, Matrix offset = Matrix::Identity, PxMaterial* material = nullptr, bool isTrigger = true);
    ~BoneCollider() override;
    void LateUpdate() override;
    Vector3 GetWorldPosition() const;
    Actor* FindOverlapActorByTag(const std::string& tag) const;

protected:
    void InitializeShape();
    void UpdateShape();
    virtual PxShape* CreateShape(PxPhysics* physics, PxMaterial* material) const = 0;
    virtual PxTransform GetLocalPose() const { return PxTransform(PxIdentity); }
    void DrawBoneSettingsGUI();

    PxShape* shape = nullptr;
    PxRigidDynamic* ghostActor = nullptr;
    PxMaterial* material = nullptr;
    Model* model = nullptr;
    int nodeIndex = -1;
    Matrix offset;
    bool isTrigger = true;
};

// ボーンを指定して追従　行列オフセットも設定できる。
class BoneSphereCollider : public BoneCollider
{
public:
    BoneSphereCollider(Object* owner, Model* model, int nodeIndex, float radius, Matrix offset = Matrix::Identity, PxMaterial* material = nullptr, bool isTrigger = true);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

private:
    PxShape* CreateShape(PxPhysics* physics, PxMaterial* material) const override;
private:
    float radius = 0.5f;
};

// ボーンを指定して追従　行列オフセットも設定できる。
class BoneCapsuleCollider : public BoneCollider
{
public:
    BoneCapsuleCollider(Object* owner, Model* model, int nodeIndex, float radius, float height, Matrix offset = Matrix::Identity, PxMaterial* material = nullptr, bool isTrigger = true);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

private:
    PxShape* CreateShape(PxPhysics* physics, PxMaterial* material) const override;
    PxTransform GetLocalPose() const override;
private:
    float radius = 0.5f;
    float height = 1.0f;
};

// ボーンを指定して追従　行列オフセットも設定できる。
class BoneBoxCollider : public BoneCollider
{
public:
    BoneBoxCollider(Object* owner, Model* model, int nodeIndex, const Vector3& size, Matrix offset = Matrix::Identity, PxMaterial* material = nullptr, bool isTrigger = true);
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

private:
    PxShape* CreateShape(PxPhysics* physics, PxMaterial* material) const override;
private:
	Vector3 size = Vector3::One;
};

class TerrainMeshCollider : public Collider
{
public:
    static constexpr int MaxResolution = 2048;

    struct CollisionArea
    {
        float minX = 0.0f, maxX = 1.0f, minZ = 0.0f, maxZ = 1.0f;
    };

    TerrainMeshCollider(Object* owner, Rigidbody* rigidbody, int resolution = 128, const CollisionArea& collisionArea = {}, PxMaterial* material = nullptr);
    ~TerrainMeshCollider() override;

    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    void RebuildFromTerrain();

private:
    void BuildMeshFromTerrain(std::vector<Vector3>& vertices, std::vector<uint32_t>& indices);
    void UpdateShape(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices);
    void ReleaseShape();
    void ClampCollisionArea();

    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;

    int resolution = 128;
    CollisionArea collisionArea;

    std::vector<Vector3> debugVertices;
    std::vector<uint32_t> debugIndices;
};
