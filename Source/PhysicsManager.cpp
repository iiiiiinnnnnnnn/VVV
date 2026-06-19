// PhysicsManager.cpp

#include "PhysicsManager.h"
#include "GameTime.h"
#include "Actor.h"
#include "Collider.h"
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

    if (PxFilterObjectIsTrigger(attr0) || PxFilterObjectIsTrigger(attr1))
    {
        pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
        return PxFilterFlag::eDEFAULT;
    }

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
    if (!actor) return nullptr;
    if (actor->IsPendingDestroy()) return nullptr;
    if (!actor->IsActive()) return nullptr;

    return actor;
}

static bool IsValidActor(Actor* actor)
{
    return actor
        && !actor->IsPendingDestroy()
        && actor->IsActive();
}

static Collider* ToCollider(PxShape* shape)
{
    if (!shape) return nullptr;

    Collider* collider = static_cast<Collider*>(shape->userData);
    if (!collider) return nullptr;

    Actor* actor = collider->GetOwnerActor();
    if (!IsValidActor(actor)) return nullptr;

    return collider;
}

static bool IsValidCollider(Collider* collider)
{
    return collider
        && collider->IsActive()
        && IsValidActor(collider->GetOwnerActor());
}

static PxTransform GetShapeGlobalPose(PxActor* actor, PxShape* shape)
{
    PxRigidActor* rigidActor = actor ? actor->is<PxRigidActor>() : nullptr;
    if (!rigidActor || !shape) return PxTransform(PxIdentity);

    return PxShapeExt::getGlobalPose(*shape, *rigidActor);
}

static void MakeFallbackPointNormal(
    PxActor* actorA,
    PxShape* shapeA,
    PxActor* actorB,
    PxShape* shapeB,
    Vector3& point,
    Vector3& normal)
{
    const Vector3 posA = VEC3(GetShapeGlobalPose(actorA, shapeA).p);
    const Vector3 posB = VEC3(GetShapeGlobalPose(actorB, shapeB).p);

    point = (posA + posB) * 0.5f;
    normal = posB - posA;
    if (normal.LengthSquared() > eps)
        normal.Normalize();
    else
        normal = Vector3::Zero;
}

static void DispatchCollisionEnter(
    Collider* a,
    Collider* b,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* actorA = a->GetOwnerActor();
    Actor* actorB = b->GetOwnerActor();
    if (!IsValidCollider(a) || !IsValidCollider(b)) return;

    actorA->OnCollisionEnter(a, b, point, normal);
    actorB->OnCollisionEnter(b, a, point, -normal);
}

static void DispatchCollisionStay(
    Collider* a,
    Collider* b,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* actorA = a->GetOwnerActor();
    Actor* actorB = b->GetOwnerActor();
    if (!IsValidCollider(a) || !IsValidCollider(b)) return;

    actorA->OnCollisionStay(a, b, point, normal);
    actorB->OnCollisionStay(b, a, point, -normal);
}

static void DispatchCollisionExit(
    Collider* a,
    Collider* b,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* actorA = a->GetOwnerActor();
    Actor* actorB = b->GetOwnerActor();
    if (!IsValidActor(actorA) || !IsValidActor(actorB)) return;

    actorA->OnCollisionExit(a, b, point, normal);
    actorB->OnCollisionExit(b, a, point, -normal);
}

static void DispatchTriggerEnter(
    Collider* trigger,
    Collider* other,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* triggerActor = trigger->GetOwnerActor();
    Actor* otherActor = other->GetOwnerActor();
    if (!IsValidCollider(trigger) || !IsValidCollider(other)) return;

    triggerActor->OnTriggerEnter(trigger, other, point, normal);
    otherActor->OnTriggerEnter(other, trigger, point, -normal);
}

static void DispatchTriggerStay(
    Collider* trigger,
    Collider* other,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* triggerActor = trigger->GetOwnerActor();
    Actor* otherActor = other->GetOwnerActor();
    if (!IsValidCollider(trigger) || !IsValidCollider(other)) return;

    triggerActor->OnTriggerStay(trigger, other, point, normal);
    otherActor->OnTriggerStay(other, trigger, point, -normal);
}

static void DispatchTriggerExit(
    Collider* trigger,
    Collider* other,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* triggerActor = trigger->GetOwnerActor();
    Actor* otherActor = other->GetOwnerActor();
    if (!IsValidActor(triggerActor) || !IsValidActor(otherActor)) return;

    triggerActor->OnTriggerExit(trigger, other, point, normal);
    otherActor->OnTriggerExit(other, trigger, point, -normal);
}

void CollisionEventCallback::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs)
{
    for (PxU32 i = 0; i < nbPairs; ++i)
    {
        const PxContactPair& cp = pairs[i];
        Collider* a = ToCollider(cp.shapes[0]);
        Collider* b = ToCollider(cp.shapes[1]);
        if (!a || !b) continue;

        Vector3 point;
        Vector3 normal;
        PxContactPairPoint contactPoint;
        if (cp.extractContacts(&contactPoint, 1) > 0)
        {
            point = VEC3(contactPoint.position);
            normal = VEC3(contactPoint.normal);
        }
        else
        {
            MakeFallbackPointNormal(
                pairHeader.actors[0],
                cp.shapes[0],
                pairHeader.actors[1],
                cp.shapes[1],
                point,
                normal);
        }

        PairState state{a, b, point, normal};

        if (cp.events & PxPairFlag::eNOTIFY_TOUCH_FOUND)
        {
            currentCollisionPairs[MakePair(a, b)] = state;
            DispatchCollisionEnter(a, b, point, normal);
        }

        if (cp.events & PxPairFlag::eNOTIFY_TOUCH_PERSISTS)
        {
            currentCollisionPairs[MakePair(a, b)] = state;
        }

        if (cp.events & PxPairFlag::eNOTIFY_TOUCH_LOST)
        {
            currentCollisionPairs.erase(MakePair(a, b));
            DispatchCollisionExit(a, b, point, normal);
        }
    }
}

void CollisionEventCallback::onTrigger(PxTriggerPair* pairs, PxU32 nbPairs)
{
    for (PxU32 i = 0; i < nbPairs; ++i)
    {
        const PxTriggerPair& tp = pairs[i];
        Collider* trigger = ToCollider(tp.triggerShape);
        Collider* other = ToCollider(tp.otherShape);
        if (!trigger || !other) continue;

        Vector3 point;
        Vector3 normal;
        MakeFallbackPointNormal(
            tp.triggerActor,
            tp.triggerShape,
            tp.otherActor,
            tp.otherShape,
            point,
            normal);

        if (tp.status & PxPairFlag::eNOTIFY_TOUCH_FOUND)
        {
            currentTriggerPairs[MakePair(trigger, other)] = {trigger, other, point, normal};
            DispatchTriggerEnter(trigger, other, point, normal);
        }

        if (tp.status & PxPairFlag::eNOTIFY_TOUCH_LOST)
        {
            currentTriggerPairs.erase(MakePair(trigger, other));
            DispatchTriggerExit(trigger, other, point, normal);
        }
    }
}

void CollisionEventCallback::DispatchStayEvents()
{
    std::erase_if(currentCollisionPairs, [](const auto& pair)
    {
        Collider* a = pair.second.a;
        Collider* b = pair.second.b;

        return !a
            || !b
            || !IsValidCollider(a)
            || !IsValidCollider(b);
    });

    std::erase_if(currentTriggerPairs, [](const auto& pair)
    {
        Collider* trigger = pair.second.a;
        Collider* other = pair.second.b;

        return !trigger
            || !other
            || !IsValidCollider(trigger)
            || !IsValidCollider(other);
    });

    for (auto& [_, state] : currentCollisionPairs)
    {
        DispatchCollisionStay(state.a, state.b, state.point, state.normal);
    }

    for (auto& [_, state] : currentTriggerPairs)
    {
        DispatchTriggerStay(state.a, state.b, state.point, state.normal);
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
    if (!owner) return;
    if (!ownerCollider) return;
    if (owner->IsPendingDestroy()) return;
    if (!owner->IsActive()) return;

    int otherLayer = static_cast<int>(hit.shape->getSimulationFilterData().word1);
    if (!Layer::Collides(ownerLayer, otherLayer)) return;

    Collider* otherCollider = ToCollider(hit.shape);
    if (!otherCollider) return;
    if (otherCollider == ownerCollider) return;

    Vector3 normal = VEC3(hit.worldNormal);
    if (normal.LengthSquared() > eps)
        normal.Normalize();
    else
        normal = Vector3::Zero;

    currentFrameColliders[otherCollider] = {VEC3(hit.worldPos), normal};
}

void CCHitReport::DispatchEvents()
{
    if (dispatchedThisFrame) return;
    dispatchedThisFrame = true;

    std::erase_if(currentFrameColliders, [](const auto& pair)
    {
        return !IsValidCollider(pair.first);
    });

    std::erase_if(prevFrameColliders, [](const auto& pair)
    {
        return !IsValidCollider(pair.first);
    });

    if (!owner || !ownerCollider || owner->IsPendingDestroy() || !owner->IsActive())
    {
        currentFrameColliders.clear();
        prevFrameColliders.clear();
        return;
    }

    for (auto& [otherCollider, state] : currentFrameColliders)
    {
        Actor* otherActor = otherCollider->GetOwnerActor();
        if (prevFrameColliders.find(otherCollider) == prevFrameColliders.end())
        {
            owner->OnCollisionEnter(ownerCollider, otherCollider, state.point, state.normal);
            otherActor->OnCollisionEnter(otherCollider, ownerCollider, state.point, -state.normal);
        }
    }

    for (auto& [otherCollider, state] : currentFrameColliders)
    {
        Actor* otherActor = otherCollider->GetOwnerActor();
        if (prevFrameColliders.find(otherCollider) != prevFrameColliders.end())
        {
            owner->OnCollisionStay(ownerCollider, otherCollider, state.point, state.normal);
            otherActor->OnCollisionStay(otherCollider, ownerCollider, state.point, -state.normal);
        }
    }

    for (auto& [otherCollider, state] : prevFrameColliders)
    {
        Actor* otherActor = otherCollider->GetOwnerActor();
        if (currentFrameColliders.find(otherCollider) == currentFrameColliders.end())
        {
            owner->OnCollisionExit(ownerCollider, otherCollider, state.point, state.normal);
            otherActor->OnCollisionExit(otherCollider, ownerCollider, state.point, -state.normal);
        }
    }

    prevFrameColliders = currentFrameColliders;
    currentFrameColliders.clear();
}
