// Rigidbody.h

#pragma once

#include "Component.h"
#include "PhysicsManager.h"
#include "Object.h"

class Rigidbody : public Component {
public:
    Rigidbody(Object* owner, PxRigidActor* actor);

    void Update() override;
    void DrawGUI() override;

    void SetPosition(const Vector3& pos);
    Vector3 GetPosition() const { return VEC(rigidActor->getGlobalPose().p); }

    PxRigidActor* GetRigidActor() const { return rigidActor; }

protected:
    PxRigidActor* rigidActor = nullptr;
};

class RigidbodyStatic : public Rigidbody {
public:
    // デフォルト: ownerのtransform.matrixから位置・回転を使う
    RigidbodyStatic(Object* owner);

    // 明示的にMatrixを渡すオーバーロード（Identityを渡すとワールド原点に配置できる）
    RigidbodyStatic(Object* owner, Matrix matrix);

    void DrawGUI() override;
};

class RigidbodyDynamic : public Rigidbody {
public:
    RigidbodyDynamic(Object* owner);

    void DrawGUI() override;

    void AddForce(const Vector3& force);
    void SetVelocity(const Vector3& v);
    const Vector3 GetVelocity() const { return VEC(rigidActor->is<PxRigidDynamic>()->getLinearVelocity()); }
};
