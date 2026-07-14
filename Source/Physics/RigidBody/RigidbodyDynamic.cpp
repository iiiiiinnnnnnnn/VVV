// RigidbodyDynamic.cpp

#include "Physics/RigidBody/RigidbodyDynamic.h"

RigidbodyDynamic::~RigidbodyDynamic()
{
    if (!rb) return;

    if (PxScene* scene = rb->getScene())
    {
        scene->removeActor(*rb);
    }

    rb->release();
    rb = nullptr;
}

void RigidbodyDynamic::OnAwake()
{
    auto* transform = owner->GetComponent<Transform>();
    _ASSERT_EXPR(transform, "Rigidbody needs transform");

    rb = PhysicsManager::Instance().CreateDynamic(transform->matrix);

    rb->userData = owner;
	SetKinematic(isKinematic);
    PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*rb);
}

void RigidbodyDynamic::OnEnabled()
{
    if (rb && rb->getScene() == nullptr)
    {
        PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*rb);
    }
}

void RigidbodyDynamic::OnDisabled()
{
    if (rb && rb->getScene() != nullptr)
    {
        PhysicsManager::Instance().GetSceneContext().GetScene()->removeActor(*rb);
    }
}

void RigidbodyDynamic::OnUpdate()
{
    if (rb->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) return;

    Transform* transform = owner->GetComponent<Transform>();
    if (!transform) return;

    PxTransform pose = rb->getGlobalPose();
    transform->SetPosition(Vector3(pose.p.x, pose.p.y, pose.p.z));
    transform->SetRotation(Quaternion(pose.q.x, pose.q.y, pose.q.z, pose.q.w));
}

void RigidbodyDynamic::OnLateUpdate()
{
    if (!(rb->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)) return;

    Transform* transform = owner->GetComponent<Transform>();
    SetPosition(transform->position);
    SetRotation(transform->rotation);
}

void RigidbodyDynamic::OnDrawGUI()
{
    if (rb)
    {
        if (ImGui::Button("Reset Velocity"))
        {
            rb->setLinearVelocity({0, 0, 0});
            rb->setAngularVelocity({0, 0, 0});
        }
    }
}

void RigidbodyDynamic::SetPosition(const Vector3& pos)
{
	if (!rb) return;

    PxTransform t = rb->getGlobalPose();
    t.p = { pos.x, pos.y, pos.z };
    rb->setGlobalPose(t);
}

void RigidbodyDynamic::SetRotation(const Quaternion& rot)
{
	if (!rb) return;

    PxTransform t = rb->getGlobalPose();
    t.q = { rot.x, rot.y, rot.z, rot.w };
    rb->setGlobalPose(t);
}

void RigidbodyDynamic::SetKinematic(bool isKinematic)
{
	this->isKinematic = isKinematic;
	if (!rb) return;

    PxRigidDynamic* dynamic = rb->is<PxRigidDynamic>();
    if (dynamic)
    {
        if (isKinematic)
        {
            dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
            dynamic->setRigidBodyFlag(PxRigidBodyFlag::eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES, true);
        }
        else
        {
            dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
            dynamic->setRigidBodyFlag(PxRigidBodyFlag::eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES, false);
        }
    }
}

void RigidbodyDynamic::AddForce(const Vector3& force)
{
    static_cast<PxRigidDynamic*>(rb)->addForce({ force.x, force.y, force.z });
}

void RigidbodyDynamic::SetVelocity(const Vector3& v)
{
    static_cast<PxRigidDynamic*>(rb)->setLinearVelocity({ v.x, v.y, v.z });
}
