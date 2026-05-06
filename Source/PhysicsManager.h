// PhysicsManager.h

#pragma once

#include "Common.h"

// px
#include "PxPhysicsAPI.h"

// core
#include "foundation/PxFoundation.h"
#include "common/PxTolerancesScale.h"
#include "geometry/PxSphereGeometry.h"
#include "geometry/PxCapsuleGeometry.h"
#include "geometry/PxBoxGeometry.h"
#include "PxRigidDynamic.h"
#include "PxRigidStatic.h"
#include "PxScene.h"
#include "PxPhysics.h"

// extension
#include "extensions/PxDefaultAllocator.h"
#include "extensions/PxDefaultErrorCallback.h"
#include "extensions/PxDefaultSimulationFilterShader.h"
#include "extensions/PxD6Joint.h"
#include "extensions/PxExtensionsAPI.h"

// character
#include "characterkinematic/PxController.h"
#include "characterkinematic/PxControllerManager.h"

// cooking
#include "cooking/PxConvexMeshDesc.h"
#include "cooking/PxTriangleMeshDesc.h"
#include "cooking/PxCooking.h"

// debug
#include "pvd/PxPvd.h"
#include "pvd/PxPvdTransport.h"
#include "pvd/PxPvdSceneClient.h"

using namespace physx;

class PhysicsSceneContext {
public:
    PhysicsSceneContext(PxVec3 gravity = PxVec3(0, -9.81f, 0));
    ~PhysicsSceneContext();
    void Simulate(float elapsedTime) const;

private:
    PxScene* scene = nullptr;
    PxControllerManager* controllerManager = nullptr;
};

class PhysicsManager {
public:
    static PhysicsManager& Instance() { static PhysicsManager instance; return instance; }

    void Initialize();
    void Finalize();

    PxPhysics* GetPhysics() { return gPhysics; }
    PxCookingParams* GetCooking() { return gCookingParams; }
    PxMaterial* GetDefaultMaterial() { return gDefaultMaterial; }
	PxDefaultCpuDispatcher* GetDispatcher() { return gDispatcher; }

private:
    PhysicsManager() = default;
    ~PhysicsManager() = default;

    PxFoundation* gFoundation = nullptr;
    PxPvd* gPvd = nullptr;
    PxPvdTransport* gPvdTransport = nullptr;
    PxDefaultCpuDispatcher* gDispatcher = nullptr;

    PxPhysics* gPhysics = nullptr;
    PxCookingParams* gCookingParams = nullptr;
    PxMaterial* gDefaultMaterial = nullptr;

    PxDefaultAllocator      gAllocator;
    PxDefaultErrorCallback  gErrorCallback;
};
