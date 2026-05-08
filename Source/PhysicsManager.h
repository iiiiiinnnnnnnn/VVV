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

    PxRigidStatic* CreateStatic(const Matrix& transform) {
        PxMat44 mat(
            PxVec4(transform._11, transform._12, transform._13, transform._14),
            PxVec4(transform._21, transform._22, transform._23, transform._24),
            PxVec4(transform._31, transform._32, transform._33, transform._34),
            PxVec4(transform._41, transform._42, transform._43, transform._44)
        );
        return gPhysics->createRigidStatic(PxTransform(mat));
    }

    PxRigidDynamic* CreateDynamic(const Matrix& transform) {
        PxMat44 mat(
            PxVec4(transform._11, transform._12, transform._13, transform._14),
            PxVec4(transform._21, transform._22, transform._23, transform._24),
            PxVec4(transform._31, transform._32, transform._33, transform._34),
            PxVec4(transform._41, transform._42, transform._43, transform._44)
        );
        PxRigidDynamic* body = gPhysics->createRigidDynamic(PxTransform(mat));
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

    PxPhysics* gPhysics = nullptr;
    PxCookingParams* gCookingParams = nullptr;
    PxMaterial* gDefaultMaterial = nullptr;

    PxDefaultAllocator      gAllocator;
    PxDefaultErrorCallback  gErrorCallback;
};
