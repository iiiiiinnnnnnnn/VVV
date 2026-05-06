// DynamicActor.h

#pragma once

#include "PhysicsActor.h"

class DynamicActor : virtual public PhysicsActor
{
public:
    DynamicActor() {
        PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
        PxRigidDynamic* dynamic = physics->createRigidDynamic(PxTransform(PxIdentity));
        SetRigidActor(dynamic);
    }
};
