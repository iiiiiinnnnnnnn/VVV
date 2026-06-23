// Rigidbody.h

#pragma once

#include "Component.h"
#include "PhysicsManager.h"
#include "Object.h"

class Rigidbody : public Component {
public:
    Rigidbody(Object* owner, PxRigidActor* actor);
    virtual ~Rigidbody();

    void Update() override;
    void DrawGUI() override;

    void SetPosition(const Vector3& pos);
    void SetRotation(const Quaternion& rot);
    Vector3 GetPosition() const { return Conv::ToVector3(rigidActor->getGlobalPose().p); }
	Quaternion GetRotation() const { return Conv::ToQuaternion(rigidActor->getGlobalPose().q); }

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
    void LateUpdate() override;

    void SetKinematic(bool isKinematic);

    void AddForce(const Vector3& force);
    void SetVelocity(const Vector3& v);
    const Vector3 GetVelocity() const { return Conv::ToVector3(rigidActor->is<PxRigidDynamic>()->getLinearVelocity()); }
};
