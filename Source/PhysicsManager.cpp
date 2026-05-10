// PhysicsManager.cpp

#include "PhysicsManager.h"

// PhysicsSceneContext

PhysicsSceneContext::PhysicsSceneContext(PxVec3 gravity)
{
	PhysicsManager& manager = PhysicsManager::Instance();

    PxSceneDesc sceneDesc(manager.GetPhysics()->getTolerancesScale());
    sceneDesc.gravity = gravity;
    sceneDesc.cpuDispatcher = manager.GetDispatcher();
    sceneDesc.filterShader = PxDefaultSimulationFilterShader;
    sceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD;
    scene = manager.GetPhysics()->createScene(sceneDesc);

    controllerManager = PxCreateControllerManager(*scene);
}

PhysicsSceneContext::~PhysicsSceneContext()
{
    if (controllerManager) controllerManager->release();
    if (scene) scene->release();
}

void PhysicsSceneContext::Simulate(float elapsedTime) const
{
    scene->simulate(elapsedTime);
    scene->fetchResults(true);
}

// PhysicsManager

void PhysicsManager::Initialize()
{
    // Foundation
    gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);

    // PVD
    gPvd = PxCreatePvd(*gFoundation);

    // 接続設定(5425番ポート)
    gPvdTransport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);

    // 接続
    if(gPvd->connect(*gPvdTransport, PxPvdInstrumentationFlag::eALL)) {
        printf("[PhysicsManager] PVD Connected!");
    }

    // Physics
    PxTolerancesScale scale;
    gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, scale, true, gPvd);

    // Extensions初期化
    PxInitExtensions(*gPhysics, gPvd);

    // Cooking
    gCookingParams =  new PxCookingParams(PxTolerancesScale());

    // CPUディスパッチャ
    gDispatcher = PxDefaultCpuDispatcherCreate(1);

    // デフォルトの物理材質（摩擦0.5, 反発0.5）
    gDefaultMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.1f);

	// シーン生成
    sceneContext = std::make_unique<PhysicsSceneContext>(PxVec3(0, -9.81f, 0));
}

void PhysicsManager::Finalize()
{
    sceneContext.reset();

    if (gDispatcher)        gDispatcher->release();
    if (gCookingParams)     delete gCookingParams;
    if (gDefaultMaterial)   gDefaultMaterial->release();
    PxCloseExtensions();
    if (gPhysics)           gPhysics->release();
    if (gPvd)               gPvd->release();
    if (gPvdTransport)      gPvdTransport->release();
    if (gFoundation)        gFoundation->release();
}
