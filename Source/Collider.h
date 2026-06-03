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
