// CharacterController.cpp

#include "CharacterController.h"
#include "Actor.h"

CharacterController::CharacterController(Object* owner, float radius, float height)
    : Component(owner)
{
    Actor* actor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(actor != nullptr, L"Object is not Actor");

    PxCapsuleControllerDesc desc;
    desc.radius = radius;
    desc.height = height;
    desc.material = PhysicsManager::Instance().GetDefaultMaterial();
    desc.position = PxExtendedVec3(
        actor->transform.position.x,
        actor->transform.position.y,
        actor->transform.position.z
    );
    desc.upDirection = PxVec3(0, 1, 0);
    desc.slopeLimit = cosf(DirectX::XMConvertToRadians(45.0f));
    desc.stepOffset = 0.3f;
    desc.contactOffset = 0.1f;

    controller = PhysicsManager::Instance()
        .GetSceneContext().GetControllerManager()->createController(desc);
}

CharacterController::~CharacterController()
{
    if (controller) controller->release();
}

void CharacterController::Update(float elapsedTime)
{
    Actor* actor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(actor != nullptr, L"Object is not Actor");

    if (!controller) return;
    PxExtendedVec3 pos = controller->getPosition();
    // カプセルの中心から足元に補正
    float halfHeight = (static_cast<PxCapsuleController*>(controller)->getHeight() * 0.5f)
        + static_cast<PxCapsuleController*>(controller)->getRadius();
    actor->transform.position = Vector3((float)pos.x, (float)pos.y - halfHeight, (float)pos.z);
}

void CharacterController::Move(const Vector3& velocity)
{
    PxControllerFilters filters;
    PxControllerCollisionFlags flags = controller->move(
        PxVec3(velocity.x, velocity.y, velocity.z),
        0.001f, 0.016f, filters
    );
    grounded = (flags & PxControllerCollisionFlag::eCOLLISION_DOWN) != PxControllerCollisionFlags(0);
}

void CharacterController::SetPosition(const Vector3& position)
{
    controller->setPosition(PxExtendedVec3(position.x, position.y, position.z));
}
