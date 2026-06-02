// CharacterController.cpp

#include "CharacterController.h"
#include "Actor.h"
#include "Graphics.h"

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

void CharacterController::Update()
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

void CharacterController::Render(const RenderContext& rc)
{
    if (rc.renderSettings.showDebug)
    {
        // PhysXカプセルはX軸基準なのでY軸に90度回転補正
        PxTransform pose = controller->getActor()->getGlobalPose();
        PxTransform offset(PxVec3(0, 0, 0), PxQuat(DirectX::XM_PIDIV2, PxVec3(0, 0, 1)));
        PxTransform corrected = pose * offset;

        Game::Graphics::Instance().GetShapeRenderer()->DrawCapsule(
            PX_TRANSFORM_TO_MATRIX(corrected),
            static_cast<PxCapsuleController*>(controller)->getRadius(),
            static_cast<PxCapsuleController*>(controller)->getHeight(),
            Color(1.0f, 1.0f, 0.0f, 1.0f)
        );
    }
}

void CharacterController::DrawGUI()
{
    if (ImGui::TreeNode("CharacterController"))
    {


        ImGui::TreePop();
    }
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
