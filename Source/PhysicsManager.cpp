// PhysicsManager.cpp

#include "PhysicsManager.h"
#include "GameTime.h"
#include "Actor.h"
#include "GameDefine.h"

static PxFilterFlags LayerFilterShader(
    PxFilterObjectAttributes attr0, PxFilterData fd0,
    PxFilterObjectAttributes attr1, PxFilterData fd1,
    PxPairFlags& pairFlags, const void*, PxU32)
{
    int layer0 = (int)fd0.word1;
    int layer1 = (int)fd1.word1;

    // マトリックスで衝突しない組み合わせなら無視
    if (!Layer::Collides(layer0, layer1))
        return PxFilterFlag::eSUPPRESS;

    pairFlags = PxPairFlag::eCONTACT_DEFAULT
        | PxPairFlag::eNOTIFY_TOUCH_FOUND
        | PxPairFlag::eNOTIFY_TOUCH_PERSISTS
        | PxPairFlag::eNOTIFY_TOUCH_LOST;
    return PxFilterFlag::eDEFAULT;
}

// -------------------------------------------------------
// CollisionEventCallback
// -------------------------------------------------------

static Actor* ToActor(PxActor* pxActor)
{
    if (!pxActor) return nullptr;
    Actor* actor = static_cast<Actor*>(pxActor->userData);
    if (!actor || actor->IsPendingDestroy()) return nullptr;
    return actor;
}

void CollisionEventCallback::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs)
{
    Actor* a = ToActor(pairHeader.actors[0]);
    Actor* b = ToActor(pairHeader.actors[1]);
    if (!a || !b) return;

    for (PxU32 i = 0; i < nbPairs; ++i)
    {
        const PxContactPair& cp = pairs[i];

        if (cp.events & PxPairFlag::eNOTIFY_TOUCH_FOUND)
        {
            currentCollisionPairs.insert(MakePair(a, b));
            a->OnCollisionEnter(b);
            b->OnCollisionEnter(a);
        }

        if (cp.events & PxPairFlag::eNOTIFY_TOUCH_LOST)
        {
            currentCollisionPairs.erase(MakePair(a, b));
            a->OnCollisionExit(b);
            b->OnCollisionExit(a);
        }
    }
}

void CollisionEventCallback::onTrigger(PxTriggerPair* pairs, PxU32 nbPairs)
{
    for (PxU32 i = 0; i < nbPairs; ++i)
    {
        const PxTriggerPair& tp = pairs[i];
        Actor* trigger = ToActor(tp.triggerActor);
        Actor* other   = ToActor(tp.otherActor);
        if (!trigger || !other) continue;

        if (tp.status & PxPairFlag::eNOTIFY_TOUCH_FOUND)
        {
            currentTriggerPairs.insert(MakePair(trigger, other));
            trigger->OnTriggerEnter(other);
            other->OnTriggerEnter(trigger);
        }

        if (tp.status & PxPairFlag::eNOTIFY_TOUCH_LOST)
        {
            currentTriggerPairs.erase(MakePair(trigger, other));
            trigger->OnTriggerExit(other);
            other->OnTriggerExit(trigger);
        }
    }
}

void CollisionEventCallback::DispatchStayEvents()
{
    // 削除済みActorをペアセットから除去
    std::erase_if(currentCollisionPairs, [](const auto& pair) {
        return pair.first->IsPendingDestroy() || pair.second->IsPendingDestroy();
    });
    std::erase_if(currentTriggerPairs, [](const auto& pair) {
        return pair.first->IsPendingDestroy() || pair.second->IsPendingDestroy();
    });

    for (auto& [a, b] : currentCollisionPairs)
    {
        a->OnCollisionStay(b);
        b->OnCollisionStay(a);
    }
    for (auto& [trigger, other] : currentTriggerPairs)
    {
        trigger->OnTriggerStay(other);
        other->OnTriggerStay(trigger);
    }
}

// -------------------------------------------------------
// PhysicsSceneContext
// -------------------------------------------------------

PhysicsSceneContext::PhysicsSceneContext(PxVec3 gravity)
{
    PhysicsManager& manager = PhysicsManager::Instance();

    PxSceneDesc sceneDesc(manager.GetPhysics()->getTolerancesScale());
    sceneDesc.gravity = gravity;
    sceneDesc.cpuDispatcher = manager.GetDispatcher();
    sceneDesc.filterShader = LayerFilterShader;
    sceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD;
    scene = manager.GetPhysics()->createScene(sceneDesc);

    // コールバック登録
    scene->setSimulationEventCallback(&eventCallback);

    controllerManager = PxCreateControllerManager(*scene);
}

PhysicsSceneContext::~PhysicsSceneContext()
{
    if (controllerManager) controllerManager->release();
    if (scene) scene->release();
}

void PhysicsSceneContext::Simulate() const
{
    scene->simulate(Game::Time::deltaTime);
    scene->fetchResults(true);

    // Stay イベントを配信（接触継続中のペアに毎フレーム通知）
    const_cast<PhysicsSceneContext*>(this)->eventCallback.DispatchStayEvents();
}

// PhysicsManager

std::unique_ptr<PhysicsSceneContext> PhysicsManager::CreateSceneContext(
    PxVec3 gravity)
{
    return std::make_unique<PhysicsSceneContext>(gravity);
}

void PhysicsManager::SetCurrentSceneContext(
    std::unique_ptr<PhysicsSceneContext> context)
{
    _ASSERT_EXPR(context != nullptr, L"PhysicsSceneContext is null.");
    sceneContext = std::move(context);
}

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
    sceneContext = CreateSceneContext(PxVec3(0, -9.81f, 0));
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

void CCHitReport::onShapeHit(const PxControllerShapeHit& hit)
{
    int otherLayer = (int)hit.shape->getSimulationFilterData().word1;
    if (!Layer::Collides(ownerLayer, otherLayer)) return;

    Actor* other = static_cast<Actor*>(hit.actor->userData);
    if (!other) return;

    // 今フレームの接触セットに追加するだけ（判定はDispatchEventsで行う）
    currentFrameActors.insert(other);
}

void CCHitReport::DispatchEvents()
{
    if (dispatchedThisFrame) return;
    dispatchedThisFrame = true;

    // 削除済みを除去
    std::erase_if(currentFrameActors, [](Actor* a) { return a->IsPendingDestroy(); });
    std::erase_if(prevFrameActors,    [](Actor* a) { return a->IsPendingDestroy(); });

    // Enter: 今フレームにいて前フレームにいなかった
    for (Actor* other : currentFrameActors)
    {
        if (prevFrameActors.find(other) == prevFrameActors.end())
        {
            owner->OnCollisionEnter(other);
            other->OnCollisionEnter(owner);
        }
    }

    // Stay: 両フレームにいる
    for (Actor* other : currentFrameActors)
    {
        if (prevFrameActors.find(other) != prevFrameActors.end())
        {
            owner->OnCollisionStay(other);
            other->OnCollisionStay(owner);
        }
    }

    // Exit: 前フレームにいて今フレームにいなかった
    for (Actor* other : prevFrameActors)
    {
        if (currentFrameActors.find(other) == currentFrameActors.end())
        {
            owner->OnCollisionExit(other);
            other->OnCollisionExit(owner);
        }
    }

    prevFrameActors = currentFrameActors;
    currentFrameActors.clear();
}
