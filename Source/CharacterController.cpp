// CharacterController.cpp

#include "CharacterController.h"
#include "Actor.h"
#include "Graphics.h"
#include "GameTime.h"
#include "IconsFontAwesome5.h"

CharacterController::CharacterController(Object* owner, float radius, float height)
    : Collider(owner)
{
    Actor* actor = Component::GetOwnerAsActor();

    hitReport = new CCHitReport(actor, this, actor->GetLayer());

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
    desc.stepOffset = 0.5f;
    desc.contactOffset = 0.1f;
    desc.reportCallback = hitReport;

    controller = PhysicsManager::Instance()
        .GetSceneContext().GetControllerManager()->createController(desc);
    SetFootPosition(actor->transform.position);

    // 内部シェイプにlayerをセット＆userDataにActor*を格納（コールバック用）
    PxRigidDynamic* act = controller->getActor();
    act->userData = actor;
    PxShape* shape = nullptr;
    act->getShapes(&shape, 1);
    shape->userData = this;
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
}

CharacterController::~CharacterController()
{
    if (controller) controller->release();
    delete hitReport;
}

void CharacterController::Update()
{
    ApplyGravity();
    SyncOwnerTransform();
}

void CharacterController::ApplyGravity()
{
    if (!useGravity) return;
    if (!controller) return;

    if (grounded)
        verticalVelocity = 0.0f;
    else
        verticalVelocity += gravity * Game::Time::deltaTime;

    Move(Vector3(0.0f, verticalVelocity * Game::Time::deltaTime, 0.0f));
}

void CharacterController::SyncOwnerTransform()
{
    Actor* actor = Component::GetOwnerAsActor();
    if (!controller) return;

    PxExtendedVec3 pos = controller->getPosition();
    float halfHeight = (static_cast<PxCapsuleController*>(controller)->getHeight() * 0.5f)
        + static_cast<PxCapsuleController*>(controller)->getRadius()
        + controller->getContactOffset();
    actor->transform.position = Vector3((float)pos.x, (float)pos.y - halfHeight, (float)pos.z);
    actor->transform.Update();
}

void CharacterController::Render(const RenderContext& rc)
{
    if (!ShouldRenderDebug()) return;
    if (!rc.renderSettings.showDebug) return;

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

void CharacterController::DrawGUI()
{
    isOpenGUI = ImGui::TreeNode(ICON_FA_STREET_VIEW " CharacterController");
    if (isOpenGUI)
    {
        PxCapsuleController* capsule = static_cast<PxCapsuleController*>(controller);

        // --- 状態表示 ---
        ImGui::Text("Grounded : %s", grounded ? "true" : "false");
        ImGui::Checkbox("Use Gravity", &useGravity);
        ImGui::DragFloat("Gravity", &gravity, 0.01f, -100.0f, 100.0f);

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
            if (ImGui::DragFloat("Step Offset", &stepOffset, 0.01f, 0.0f, 1.5f))
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
    CCShapeFilterCallback(Actor* owner, int ownerLayer)
        : owner(owner), ownerLayer(ownerLayer) {}

    PxQueryHitType::Enum preFilter(
        const PxFilterData& filterData, const PxShape* shape,
        const PxRigidActor* rigidActor, PxHitFlags&) override
    {
        if (rigidActor && rigidActor->userData == owner) return PxQueryHitType::eNONE;

        int layer = (int)shape->getSimulationFilterData().word1;
        if (!Layer::Collides(ownerLayer, layer)) return PxQueryHitType::eNONE;
        return PxQueryHitType::eBLOCK;
    }
    PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&, const PxShape*, const PxRigidActor*) override
    {
        return PxQueryHitType::eBLOCK;
    }

private:
    Actor* owner = nullptr;
    int ownerLayer = 0;
};

void CharacterController::Move(const Vector3& velocity)
{
    static CCFilterCallback      ccFilter;
    Actor* actor = Component::GetOwnerAsActor();
    CCShapeFilterCallback shapeFilter(actor, actor->GetLayer());

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
    if (grounded && velocity.y <= 0.0f)
        verticalVelocity = 0.0f;

    SyncOwnerTransform();

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
    verticalVelocity = 0.0f;
    SyncOwnerTransform();
}

void CharacterController::SetFootPosition(const Vector3& position)
{
    PxCapsuleController* capsule = static_cast<PxCapsuleController*>(controller);
    const float halfHeight =
        (capsule->getHeight() * 0.5f) +
        capsule->getRadius() +
        controller->getContactOffset();

    controller->setPosition(PxExtendedVec3(
        position.x,
        position.y + halfHeight,
        position.z));
    verticalVelocity = 0.0f;
    SyncOwnerTransform();
}
