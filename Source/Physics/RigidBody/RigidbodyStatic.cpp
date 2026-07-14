// RigidbodyStatic.cpp

#include "Physics/RigidBody/RigidbodyStatic.h"

RigidbodyStatic::~RigidbodyStatic()
{
    if (!rb) return;

    if (PxScene* scene = rb->getScene())
    {
        scene->removeActor(*rb);
    }

    rb->release();
    rb = nullptr;
}

void RigidbodyStatic::OnAwake()
{
    auto* transform = owner->GetComponent<Transform>();
    _ASSERT_EXPR(transform, "Rigidbody needs transform");

    rb = PhysicsManager::Instance().CreateStatic(transform->matrix);

    rb->userData = owner;
    PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*rb);
}

void RigidbodyStatic::OnEnabled()
{
    if (rb && rb->getScene() == nullptr)
    {
        PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*rb);
    }
}

void RigidbodyStatic::OnDisabled()
{
    if (rb && rb->getScene() != nullptr)
    {
        PhysicsManager::Instance().GetSceneContext().GetScene()->removeActor(*rb);
    }
}

void RigidbodyStatic::SetPosition(const Vector3& pos)
{
	if (!rb) return;

    PxTransform t = rb->getGlobalPose();
    t.p = { pos.x, pos.y, pos.z };
    rb->setGlobalPose(t);
}

void RigidbodyStatic::SetRotation(const Quaternion& rot)
{
	if (!rb) return;

    PxTransform t = rb->getGlobalPose();
    t.q = { rot.x, rot.y, rot.z, rot.w };
    rb->setGlobalPose(t);
}
