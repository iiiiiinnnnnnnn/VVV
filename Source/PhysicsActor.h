// PhysicsActor.h

#pragma once

#include "Actor.h"
#include "PhysicsManager.h"

class PhysicsActor : virtual public Actor
{
public:
    void SetPosition(const Vector3& pos) {
        PxTransform transform = rigidActor->getGlobalPose();
        transform.p = { pos.x, pos.y, pos.z };
        rigidActor->setGlobalPose(transform);
    }

    Vector3 GetPosition() const {
        return VEC(rigidActor->getGlobalPose().p);
    }

    virtual void OnCollisionEnter(Actor* other) {}
    virtual void OnCollisionExit(Actor* other) {}
    virtual void OnTriggerEnter(Actor* other) {}
    virtual void OnTriggerExit(Actor* other) {}

protected:
    void SetRigidActor(PxRigidActor* actor) { rigidActor = actor; }
    PxRigidActor* GetRigidActor() const { return rigidActor; }
    
private:
    PxRigidActor* rigidActor = nullptr;
};
