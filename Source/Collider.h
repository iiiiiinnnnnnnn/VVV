// Collider.h

#pragma once

#include "Component.h"
#include "PhysicsManager.h"
#include "Actor.h"
#include "Model.h"
#include "Rigidbody.h"

class BoxCollider : public Component {
public:
    BoxCollider(Actor* owner, Rigidbody* rigidbody, const Vector3& size, PxMaterial* material = nullptr);
    void DrawGUI(float elapsedTime) override;

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
    CapsuleCollider(Actor* owner, Rigidbody* rigidbody, float radius, float height, PxMaterial* material = nullptr);
    void DrawGUI(float elapsedTime) override;

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
    SphereCollider(Actor* owner, Rigidbody* rigidbody, float radius, PxMaterial* material = nullptr);
    void DrawGUI(float elapsedTime) override;

    const float GetRadius() const { return radius; }
private:
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    float radius;
};

class MeshCollider : public Component {
public:
    MeshCollider(Actor* owner, Rigidbody* rigidbody, Model* model, PxMaterial* material = nullptr);
    void DrawGUI(float elapsedTime) override;
private:
    void UpdateShape();
    PxShape* shape = nullptr;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    Model* model = nullptr;
};
