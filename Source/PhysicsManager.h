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

    PxScene* GetScene() const { return scene; }
    PxControllerManager* GetControllerManager() const { return controllerManager; }

private:
    PxScene* scene = nullptr;
    PxControllerManager* controllerManager = nullptr;
};

class PhysicsManager {
public:
    static PhysicsManager& Instance() { static PhysicsManager instance; return instance; }

    void Initialize();
    void Finalize();

    PhysicsSceneContext& GetSceneContext() { return *sceneContext; }

    PxPhysics* GetPhysics() { return gPhysics; }
    PxCookingParams* GetCooking() { return gCookingParams; }
    PxMaterial* GetDefaultMaterial() { return gDefaultMaterial; }
	PxDefaultCpuDispatcher* GetDispatcher() { return gDispatcher; }

    PxRigidStatic* CreateStatic(Matrix& transform) {
        PxTransform t;
        Vector3 scale, pos;
        Quaternion rot;
        transform.Decompose(scale, rot, pos);
        t.p = PxVec3(pos.x, pos.y, pos.z);
        t.q = PxQuat(rot.x, rot.y, rot.z, rot.w);
        return gPhysics->createRigidStatic(t);
    }

    PxRigidDynamic* CreateDynamic(Matrix& transform) {
        PxTransform t;
        Vector3 scale, pos;
        Quaternion rot;
        transform.Decompose(scale, rot, pos);
        t.p = PxVec3(pos.x, pos.y, pos.z);
        t.q = PxQuat(rot.x, rot.y, rot.z, rot.w);
        PxRigidDynamic* body = gPhysics->createRigidDynamic(t);
        PxRigidBodyExt::updateMassAndInertia(*body, 1.0f);
        return body;
    }

private:
    PhysicsManager() = default;
    ~PhysicsManager() = default;

    PxFoundation* gFoundation = nullptr;
    PxPvd* gPvd = nullptr;
    PxPvdTransport* gPvdTransport = nullptr;
    PxDefaultCpuDispatcher* gDispatcher = nullptr;

    std::unique_ptr<PhysicsSceneContext> sceneContext;

    PxPhysics* gPhysics = nullptr;
    PxCookingParams* gCookingParams = nullptr;
    PxMaterial* gDefaultMaterial = nullptr;

    PxDefaultAllocator      gAllocator;
    PxDefaultErrorCallback  gErrorCallback;
};
