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
    float halfHeight = (static_cast<PxCapsuleController*>(controller)->getHeight() * 0.5f)
        + static_cast<PxCapsuleController*>(controller)->getRadius()
        + controller->getContactOffset();
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
            static_cast<PxCapsuleController*>(controller)->getRadius() + controller->getContactOffset(),
            static_cast<PxCapsuleController*>(controller)->getHeight(),
            Color(1.0f, 1.0f, 0.0f, 1.0f)
        );
    }
}

void CharacterController::DrawGUI()
{
    if (ImGui::TreeNode("CharacterController"))
    {
        PxCapsuleController* capsule = static_cast<PxCapsuleController*>(controller);

        // --- 状態表示 ---
        ImGui::Text("Grounded : %s", grounded ? "true" : "false");

        PxExtendedVec3 pos = controller->getPosition();
        ImGui::Text("Position : (%.2f, %.2f, %.2f)", (float)pos.x, (float)pos.y, (float)pos.z);

        // --- カプセル形状 ---
        if (ImGui::TreeNode("Shape"))
        {
            float radius = capsule->getRadius();
            float height = capsule->getHeight();

            if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 10.0f))
                capsule->setRadius(radius);
            if (ImGui::DragFloat("Height", &height, 0.01f, 0.01f, 10.0f))
                capsule->setHeight(height);

            ImGui::TreePop();
        }

        // --- コントローラ設定 ---
        if (ImGui::TreeNode("Settings"))
        {
            float stepOffset = controller->getStepOffset();
            if (ImGui::DragFloat("Step Offset", &stepOffset, 0.01f, 0.0f, 1.0f))
                controller->setStepOffset(stepOffset);

            float contactOffset = controller->getContactOffset();
            ImGui::Text("Contact Offset : %.3f", contactOffset);

            // slopeLimit は角度(deg)で表示・編集して内部はcos値に変換
            float slopeDeg = acosf(controller->getSlopeLimit()) * RAD2DEG;
            if (ImGui::DragFloat("Slope Limit (deg)", &slopeDeg, 0.5f, 0.0f, 90.0f))
                controller->setSlopeLimit(cosf(slopeDeg * DEG2RAD));

            ImGui::TreePop();
        }

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