// Collider.h

#pragma once

#include "Component.h"
#include "PhysicsManager.h"
#include "Actor.h"

class Collider : public Component {
public:
    Collider(Actor* owner);
    void Update(float elapsedTime) override;
    void DrawGUI(float elapsedTime) override;
};

class BoxCollider : public Collider {
public:
    BoxCollider(Actor* owner, const Vector3& size);
    void Update(float elapsedTime) override;
    void DrawGUI(float elapsedTime) override;

	const Vector3& GetSize() const { return size; }

private:
	Vector3 size;
};

class CapsuleCollider : public Collider {
public:
    CapsuleCollider(Actor* owner, float radius, float height);
    void Update(float elapsedTime) override;
    void DrawGUI(float elapsedTime) override;

    const float GetRadius() const { return radius; }
    const float GetHeight() const { return height; }
private:
    float radius;
    float height;
};

class SphereCollider : public Collider {
public:
    SphereCollider(Actor* owner, float radius);
    void Update(float elapsedTime) override;
    void DrawGUI(float elapsedTime) override;

    const float GetRadius() const { return radius; }
private:
    float radius;
};
