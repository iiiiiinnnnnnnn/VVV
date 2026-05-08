// Rigidbody.h

#pragma once

#include "Component.h"
#include "PhysicsManager.h"
#include "Actor.h"

class Rigidbody : public Component {
public:
    Rigidbody(Actor* owner, PxRigidActor* actor);

    void Update(float elapsedTime) override;

    void DrawGUI(float elapsedTime) override;

    void SetPosition(const Vector3& pos);

    Vector3 GetPosition() const { return VEC(rigidActor->getGlobalPose().p); }

    PxRigidActor* GetRigidActor() const { return rigidActor; }

protected:
    PxRigidActor* rigidActor = nullptr;
};

class StaticRigidbody : public Rigidbody {
public:
    StaticRigidbody(Actor* owner);

    void DrawGUI(float elapsedTime) override;
};

class DynamicRigidbody : public Rigidbody {
public:
    DynamicRigidbody(Actor* owner);

    void DrawGUI(float elapsedTime) override;

    void AddForce(const Vector3& force);

    void SetVelocity(const Vector3& v);
};
