// StaticActor.h

#pragma once

#include "PhysicsActor.h"

class StaticActor : virtual public PhysicsActor
{
public:
    StaticActor() {
        PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
        PxRigidStatic* staticActor = physics->createRigidStatic(PxTransform(PxIdentity));
        SetRigidActor(staticActor);
    }
};
