// PhysicsManager.cpp

#include "Physics/Core/PhysicsManager.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Actor.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Application/SettingsAndDebug/PhysicsLayerManager.h"

static PxFilterFlags LayerFilterShader(
    PxFilterObjectAttributes attr0, PxFilterData fd0,
    PxFilterObjectAttributes attr1, PxFilterData fd1,
    PxPairFlags& pairFlags, const void*, PxU32)
{
    int layer0 = (int)fd0.word1;
    int layer1 = (int)fd1.word1;

    // マトリックスで衝突しない組み合わせなら無視
    if (!PhysicsLayerManager::Instance().Collides(layer0, layer1))
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

static PhysicsComponent* ToCollider(PxShape* shape)
{
    if (!shape) return nullptr;

    PhysicsComponent* collider = static_cast<PhysicsComponent*>(shape->userData);
    if (!PhysicsComponent::IsLive(collider) || !collider->IsActive()) return nullptr;

    Actor* actor = dynamic_cast<Actor*>(collider->GetOwner());
    if (!IsValidActor(actor)) return nullptr;

    return collider;
}

static bool IsValidCollider(PhysicsComponent* collider)
{
    return PhysicsComponent::IsLive(collider)
        && collider->IsActive()
        && IsValidActor(dynamic_cast<Actor*>(collider->GetOwner()));
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
    const Vector3 posA = Conv::ToVector3(GetShapeGlobalPose(actorA, shapeA).p);
    const Vector3 posB = Conv::ToVector3(GetShapeGlobalPose(actorB, shapeB).p);

    point = (posA + posB) * 0.5f;
    normal = posB - posA;
    if (normal.LengthSquared() > eps)
        normal.Normalize();
    else
        normal = Vector3::Zero;
}

static void DispatchCollisionEnter(
    PhysicsComponent* a,
    PhysicsComponent* b,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* actorA = dynamic_cast<Actor*>(a->GetOwner());
    Actor* actorB = dynamic_cast<Actor*>(b->GetOwner());
    if (!IsValidCollider(a) || !IsValidCollider(b)) return;

    actorA->OnCollisionEnter(a, b, point, normal);
    actorB->OnCollisionEnter(b, a, point, -normal);
}

static void DispatchCollisionStay(
    PhysicsComponent* a,
    PhysicsComponent* b,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* actorA = dynamic_cast<Actor*>(a->GetOwner());
    Actor* actorB = dynamic_cast<Actor*>(b->GetOwner());
    if (!IsValidCollider(a) || !IsValidCollider(b)) return;

    actorA->OnCollisionStay(a, b, point, normal);
    actorB->OnCollisionStay(b, a, point, -normal);
}

static void DispatchCollisionExit(
    PhysicsComponent* a,
    PhysicsComponent* b,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* actorA = dynamic_cast<Actor*>(a->GetOwner());
    Actor* actorB = dynamic_cast<Actor*>(b->GetOwner());
    if (!IsValidActor(actorA) || !IsValidActor(actorB)) return;

    actorA->OnCollisionExit(a, b, point, normal);
    actorB->OnCollisionExit(b, a, point, -normal);
}

static void DispatchTriggerEnter(
    PhysicsComponent* trigger,
    PhysicsComponent* other,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* triggerActor = dynamic_cast<Actor*>(trigger->GetOwner());
    Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());
    if (!IsValidCollider(trigger) || !IsValidCollider(other)) return;

    triggerActor->OnTriggerEnter(trigger, other, point, normal);
    otherActor->OnTriggerEnter(other, trigger, point, -normal);
}

static void DispatchTriggerStay(
    PhysicsComponent* trigger,
    PhysicsComponent* other,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* triggerActor = dynamic_cast<Actor*>(trigger->GetOwner());
    Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());
    if (!IsValidCollider(trigger) || !IsValidCollider(other)) return;

    triggerActor->OnTriggerStay(trigger, other, point, normal);
    otherActor->OnTriggerStay(other, trigger, point, -normal);
}

static void DispatchTriggerExit(
    PhysicsComponent* trigger,
    PhysicsComponent* other,
    const Vector3& point,
    const Vector3& normal)
{
    Actor* triggerActor = dynamic_cast<Actor*>(trigger->GetOwner());
    Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());
    if (!IsValidActor(triggerActor) || !IsValidActor(otherActor)) return;

    triggerActor->OnTriggerExit(trigger, other, point, normal);
    otherActor->OnTriggerExit(other, trigger, point, -normal);
}

void CollisionEventCallback::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs)
{
    for (PxU32 i = 0; i < nbPairs; ++i)
    {
        const PxContactPair& cp = pairs[i];
        if (cp.flags & PxContactPairFlag::eREMOVED_SHAPE_0)
        {
            continue;
        }

        if (cp.flags & PxContactPairFlag::eREMOVED_SHAPE_1)
        {
            continue;
        }

        PhysicsComponent* a = ToCollider(cp.shapes[0]);
        PhysicsComponent* b = ToCollider(cp.shapes[1]);

        if (!a || !b)
        {
            continue;
        }

        Vector3 point;
        Vector3 normal;
        PxContactPairPoint contactPoint;
        if (cp.extractContacts(&contactPoint, 1) > 0)
        {
            point = Conv::ToVector3(contactPoint.position);
            normal = Conv::ToVector3(contactPoint.normal);
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

        if (tp.flags & PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER)
        {
            continue;
        }

        if (tp.flags & PxTriggerPairFlag::eREMOVED_SHAPE_OTHER)
        {
            continue;
        }

        PhysicsComponent* trigger = ToCollider(tp.triggerShape);
        PhysicsComponent* other = ToCollider(tp.otherShape);

        if (!trigger || !other)
        {
            continue;
        }

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
        PhysicsComponent* a = pair.second.a;
        PhysicsComponent* b = pair.second.b;

        return !a
            || !b
            || !IsValidCollider(a)
            || !IsValidCollider(b);
    });

    std::erase_if(currentTriggerPairs, [](const auto& pair)
    {
        PhysicsComponent* trigger = pair.second.a;
        PhysicsComponent* other = pair.second.b;

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

void CollisionEventCallback::ClearPairs()
{
    currentCollisionPairs.clear();
    currentTriggerPairs.clear();
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

bool PhysicsManager::Raycast(
    const Vector3& origin,
    const Vector3& direction,
    float distance,
    PhysicsRaycastHit& hit,
    int layer) const
{
    return Raycast(origin, direction, distance, hit, layer, nullptr);
}

bool PhysicsManager::Raycast(
    const Vector3& origin,
    const Vector3& direction,
    float distance,
    PhysicsRaycastHit& hit,
    int layer,
    const Actor* ignoreActor) const
{
    if (distance <= 0.001f)
    {
        return false;
    }

    Vector3 rayDirection = direction;

    if (rayDirection.LengthSquared() < 0.000001f)
    {
        return false;
    }

    rayDirection.Normalize();

    physx::PxVec3 pxOrigin(
        origin.x,
        origin.y,
        origin.z
    );

    physx::PxVec3 pxDirection(
        rayDirection.x,
        rayDirection.y,
        rayDirection.z
    );

    physx::PxRaycastBuffer hitBuffer;
    physx::PxQueryFilterData filterData;
    if (layer >= 0 || ignoreActor)
    {
        filterData.flags |= physx::PxQueryFlag::ePREFILTER;
    }

    struct RaycastFilterCallback : physx::PxQueryFilterCallback
    {
        int layer = -1;
        const Actor* ignoreActor = nullptr;

        physx::PxQueryHitType::Enum preFilter(
            const physx::PxFilterData&,
            const physx::PxShape* shape,
            const physx::PxRigidActor* rigidActor,
            physx::PxHitFlags&) override
        {
            if (ignoreActor && rigidActor && rigidActor->userData == ignoreActor) return physx::PxQueryHitType::eNONE;
            if (layer < 0) return physx::PxQueryHitType::eBLOCK;
            if (!shape) return physx::PxQueryHitType::eNONE;

            int otherLayer = static_cast<int>(shape->getSimulationFilterData().word1);
            if (!PhysicsLayerManager::Instance().Collides(layer, otherLayer)) return physx::PxQueryHitType::eNONE;
            return physx::PxQueryHitType::eBLOCK;
        }

        physx::PxQueryHitType::Enum postFilter(
            const physx::PxFilterData&,
            const physx::PxQueryHit&,
            const physx::PxShape*,
            const physx::PxRigidActor*) override
        {
            return physx::PxQueryHitType::eBLOCK;
        }
    } filterCallback;

    filterCallback.layer = layer;
    filterCallback.ignoreActor = ignoreActor;

    bool result = GetSceneContext().GetScene()->raycast(
        pxOrigin,
        pxDirection,
        distance,
        hitBuffer,
        physx::PxHitFlag::eDEFAULT,
        filterData,
        (layer >= 0 || ignoreActor) ? &filterCallback : nullptr
    );

    if (!result || !hitBuffer.hasBlock)
    {
        return false;
    }

    const physx::PxRaycastHit& block = hitBuffer.block;

    hit.position = Vector3(
        block.position.x,
        block.position.y,
        block.position.z
    );

    hit.normal = Vector3(
        block.normal.x,
        block.normal.y,
        block.normal.z
    );

    hit.distance = block.distance;
    hit.layerId =
        block.shape
        ? static_cast<LayerId>(block.shape->getQueryFilterData().word1)
        : InvalidLayerId;

    return true;
}

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

void PhysicsManager::RefreshLayerFiltering()
{
    PxScene* scene = GetSceneContext().GetScene();
    if (!scene) return;

    const PxActorTypeFlags actorTypes = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;
    const PxU32 actorCount = scene->getNbActors(actorTypes);
    std::vector<PxActor*> actors(actorCount);
    if (actorCount > 0) scene->getActors(actorTypes, actors.data(), actorCount);
    GetSceneContext().ClearCollisionEvents();
    for (PxActor* actor : actors)
    {
        if (actor) scene->resetFiltering(*actor);
    }
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
    if (!PhysicsLayerManager::Instance().Collides(ownerLayer, otherLayer)) return;

    PhysicsComponent* otherCollider = ToCollider(hit.shape);
    if (!otherCollider) return;
    if (otherCollider == ownerCollider) return;

    Vector3 normal = Conv::ToVector3(hit.worldNormal);
    if (normal.LengthSquared() > eps)
        normal.Normalize();
    else
        normal = Vector3::Zero;

    currentFrameColliders[otherCollider] = {Conv::ToVector3(hit.worldPos), normal};
}

void CCHitReport::onControllerHit(const PxControllersHit& hit)
{
	if (!owner || !ownerCollider || !hit.other) return;
	if (owner->IsPendingDestroy() || !owner->IsActive()) return;

	PxShape* otherShape = nullptr;
	hit.other->getActor()->getShapes(&otherShape, 1);
	PhysicsComponent* otherCollider = ToCollider(otherShape);
	if (!otherCollider || otherCollider == ownerCollider) return;

	const int otherLayer = static_cast<int>(otherShape->getSimulationFilterData().word1);
	if (!PhysicsLayerManager::Instance().Collides(ownerLayer, otherLayer)) return;

	Vector3 normal = Conv::ToVector3(hit.worldNormal);
	if (normal.LengthSquared() > eps) normal.Normalize();
	else normal = Vector3::Zero;

	currentFrameColliders[otherCollider] = {Conv::ToVector3(hit.worldPos), normal};
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
        Actor* otherActor = dynamic_cast<Actor*>(otherCollider->GetOwner());
        if (prevFrameColliders.find(otherCollider) == prevFrameColliders.end())
        {
            owner->OnCollisionEnter(ownerCollider, otherCollider, state.point, state.normal);
            otherActor->OnCollisionEnter(otherCollider, ownerCollider, state.point, -state.normal);
        }
    }

    for (auto& [otherCollider, state] : currentFrameColliders)
    {
        Actor* otherActor = dynamic_cast<Actor*>(otherCollider->GetOwner());
        if (prevFrameColliders.find(otherCollider) != prevFrameColliders.end())
        {
            owner->OnCollisionStay(ownerCollider, otherCollider, state.point, state.normal);
            otherActor->OnCollisionStay(otherCollider, ownerCollider, state.point, -state.normal);
        }
    }

    for (auto& [otherCollider, state] : prevFrameColliders)
    {
        Actor* otherActor = dynamic_cast<Actor*>(otherCollider->GetOwner());
        if (currentFrameColliders.find(otherCollider) == currentFrameColliders.end())
        {
            owner->OnCollisionExit(ownerCollider, otherCollider, state.point, state.normal);
            otherActor->OnCollisionExit(otherCollider, ownerCollider, state.point, -state.normal);
        }
    }

    prevFrameColliders = currentFrameColliders;
    currentFrameColliders.clear();
}
