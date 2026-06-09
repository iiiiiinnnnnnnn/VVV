// CharacterController.cpp

#include "CharacterController.h"
#include "Actor.h"
#include "Graphics.h"
#include "GameTime.h"

CharacterController::CharacterController(Object* owner, float radius, float height)
    : Component(owner)
{
    Actor* actor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(actor != nullptr, L"Object is not Actor");

    hitReport = new CCHitReport(actor, actor->GetLayer());

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
    desc.reportCallback = hitReport;

    controller = PhysicsManager::Instance()
        .GetSceneContext().GetControllerManager()->createController(desc);

    // 内部シェイプにlayerをセット＆userDataにActor*を格納（コールバック用）
    PxRigidDynamic* act = controller->getActor();
    act->userData = actor;
    PxShape* shape = nullptr;
    act->getShapes(&shape, 1);
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
}

CharacterController::~CharacterController()
{
    if (controller) controller->release();
    delete hitReport;
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

// CCShape filter: uses Layer::CollisionMatrix to automatically exclude layers
// that should not block the player (e.g. Hair, Body). No hardcoding needed.
struct CCShapeFilterCallback : public PxQueryFilterCallback
{
    PxQueryHitType::Enum preFilter(
        const PxFilterData& filterData, const PxShape* shape,
        const PxRigidActor*, PxHitFlags&) override
    {
        int layer = (int)shape->getSimulationFilterData().word1;
        if (!Layer::Collides(Layer::Player, layer)) return PxQueryHitType::eNONE;
        return PxQueryHitType::eBLOCK;
    }
    PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&, const PxShape*, const PxRigidActor*) override
    {
        return PxQueryHitType::eBLOCK;
    }
};

void CharacterController::Move(const Vector3& velocity)
{
    static CCFilterCallback      ccFilter;
    static CCShapeFilterCallback shapeFilter;

    // 新フレームの開始としてフラグをリセット（LateUpdateより先にMoveが呼ばれる想定）
    hitReport->dispatchedThisFrame = false;

    PxControllerFilters filters;
    filters.mCCTFilterCallback = &ccFilter;
    filters.mFilterCallback = &shapeFilter;

    PxControllerCollisionFlags flags = controller->move(
        PxVec3(velocity.x, velocity.y, velocity.z),
        0.001f, Game::Time::deltaTime, filters
    );
    grounded = (flags & PxControllerCollisionFlag::eCOLLISION_DOWN) != PxControllerCollisionFlags(0);

    hitReport->DispatchEvents();
}

void CharacterController::LateUpdate()
{
    // Move()はOnLateUpdateで呼ばれるため、その後に判定する
    hitReport->DispatchEvents();
}

void CharacterController::SetPosition(const Vector3& position)
{
    controller->setPosition(PxExtendedVec3(position.x, position.y, position.z));
}