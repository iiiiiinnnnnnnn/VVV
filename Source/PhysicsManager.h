// PhysicsManager.h

#pragma once

#include "Common.h"
#include <set>

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

#include "GameDefine.h"

using namespace physx;

static constexpr PxU32 LayerMask(int layer) { return (1u << layer); }

class Actor;
class Collider;

// 衝突イベントコールバック
class CollisionEventCallback : public PxSimulationEventCallback
{
public:
    void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override;
    void onTrigger(PxTriggerPair* pairs, PxU32 nbPairs) override;
    void DispatchStayEvents();
    void ClearPairs();

    void onConstraintBreak(PxConstraintInfo*, PxU32) override {}
    void onWake(PxActor**, PxU32) override {}
    void onSleep(PxActor**, PxU32) override {}
    void onAdvance(const PxRigidBody* const*, const PxTransform*, PxU32) override {}

private:
    using ColliderPair = std::pair<Collider*, Collider*>;
    struct PairState
    {
        Collider* a = nullptr;
        Collider* b = nullptr;
        Vector3 point = Vector3::Zero;
        Vector3 normal = Vector3::Zero;
    };

    std::map<ColliderPair, PairState> currentCollisionPairs;
    std::map<ColliderPair, PairState> currentTriggerPairs;
    static ColliderPair MakePair(Collider* a, Collider* b)
    {
        return (a <= b) ? ColliderPair(a, b) : ColliderPair(b, a);
    }
};

// CharacterController の接触コールバック（CC対Rigidbody）
class CCHitReport : public PxUserControllerHitReport
{
public:
    CCHitReport(Actor* owner, Collider* ownerCollider, int layer)
        : owner(owner), ownerCollider(ownerCollider), ownerLayer(layer) {}

    void onShapeHit(const PxControllerShapeHit& hit) override;
    void onControllerHit(const PxControllersHit& hit) override {}
    void onObstacleHit(const PxControllerObstacleHit& hit) override {}

    // Framework の Simulate 後に毎フレーム呼ぶ
    void DispatchEvents();

private:
    Actor* owner = nullptr;
    Collider* ownerCollider = nullptr;
    int ownerLayer = 0;
    struct HitState
    {
        Vector3 point = Vector3::Zero;
        Vector3 normal = Vector3::Zero;
    };
    std::map<Collider*, HitState> currentFrameColliders;
    std::map<Collider*, HitState> prevFrameColliders;
public:
    bool dispatchedThisFrame = false;
};

// CharacterController同士の衝突フィルタ
class CCFilterCallback : public PxControllerFilterCallback
{
public:
    bool filter(const PxController& a, const PxController& b) override
    {
        PxShape* shapeA = nullptr; a.getActor()->getShapes(&shapeA, 1);
        PxShape* shapeB = nullptr; b.getActor()->getShapes(&shapeB, 1);
        int layerA = (int)shapeA->getSimulationFilterData().word1;
        int layerB = (int)shapeB->getSimulationFilterData().word1;
        return Layer::Collides(layerA, layerB);
    }
};

class PhysicsSceneContext {
public:
    PhysicsSceneContext(PxVec3 gravity = PxVec3(0, -9.81f, 0));
    ~PhysicsSceneContext();
    void Simulate() const;

    PxScene* GetScene() const { return scene; }
    PxControllerManager* GetControllerManager() const { return controllerManager; }
    CollisionEventCallback& GetEventCallback() { return eventCallback; }
    void ClearCollisionEvents() { eventCallback.ClearPairs(); }

private:
    PxScene* scene = nullptr;
    PxControllerManager* controllerManager = nullptr;
    CollisionEventCallback eventCallback;
};

class PhysicsManager {
public:
    static PhysicsManager& Instance() { static PhysicsManager instance; return instance; }

    void Initialize();
    void Finalize();

    PhysicsSceneContext& GetSceneContext()
    {
        PhysicsSceneContext* context = threadSceneContext
            ? threadSceneContext
            : sceneContext.get();

        _ASSERT_EXPR(context != nullptr, L"PhysicsSceneContext is null.");
        return *context;
    }

    std::unique_ptr<PhysicsSceneContext> CreateSceneContext(
        PxVec3 gravity = PxVec3(0, -9.81f, 0));

    void SetCurrentSceneContext(
        std::unique_ptr<PhysicsSceneContext> context);

    // 非同期ロードスレッドだけが使用するPhysXシーンを指定する。
    // nullptrを渡すと通常のカレントシーンへ戻る。
    void SetThreadSceneContext(PhysicsSceneContext* context)
    {
        threadSceneContext = context;
    }

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

    static void SetLayerToShape(PxShape* shape, int layer)
    {
        PxFilterData fd;
        fd.word0 = (1u << layer); // 自分のレイヤービット
        fd.word1 = (PxU32)layer;  // レイヤー番号（FilterShaderで参照）
        shape->setSimulationFilterData(fd);
        shape->setQueryFilterData(fd);
    }

private:
    PhysicsManager() = default;
    ~PhysicsManager() = default;

    PxFoundation* gFoundation = nullptr;
    PxPvd* gPvd = nullptr;
    PxPvdTransport* gPvdTransport = nullptr;
    PxDefaultCpuDispatcher* gDispatcher = nullptr;

    std::unique_ptr<PhysicsSceneContext> sceneContext;
    inline static thread_local PhysicsSceneContext* threadSceneContext = nullptr;

    PxPhysics* gPhysics = nullptr;
    PxCookingParams* gCookingParams = nullptr;
    PxMaterial* gDefaultMaterial = nullptr;

    PxDefaultAllocator      gAllocator;
    PxDefaultErrorCallback  gErrorCallback;
};
