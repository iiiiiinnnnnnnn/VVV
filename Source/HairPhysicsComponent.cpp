// HairPhysicsComponent.cpp

#include "HairPhysicsComponent.h"
#include "Actor.h"
#include "Graphics.h"

// ---------------------------------------------------------------------------
// コンストラクタ
// ---------------------------------------------------------------------------
HairPhysicsComponent::HairPhysicsComponent(
    Object* owner,
    Model* model,
    const std::vector<std::string>& boneNames,
    float sphereRadius,
    float mass)
    : Component(owner)
    , model(model)
    , sphereRadius(sphereRadius)
{
    for (auto& node : model->GetNodes())
    {
        for (const auto& name : boneNames)
        {
            if (node.name == name)
            {
                BuildUnit(const_cast<Model::Node*>(&node), sphereRadius, mass);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// デストラクタ
// ---------------------------------------------------------------------------
HairPhysicsComponent::~HairPhysicsComponent()
{
    for (auto& u : units)
        DestroyUnit(u);
}

// ---------------------------------------------------------------------------
// LateUpdate: Simulate前にanchorをアニメ後ボーン位置へ追従
// ---------------------------------------------------------------------------
void HairPhysicsComponent::LateUpdate()
{
    for (auto& u : units)
    {
        if (!u.node || !u.anchor) continue;
        PxTransform t = MATRIX_TO_PX_TRANSFORM(u.node->worldTransform);
        u.anchor->setKinematicTarget(t);
    }
}

// ---------------------------------------------------------------------------
// Render: Simulate後にhair剛体の結果をボーンへ書き戻す
// Render()はFramework::Render()から呼ばれるためSimulate()確実に後になる
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ApplySimulationResult: Simulate後にhair剛体の結果をボーンね書き戻す
// (Scene::UpdateからSimulate直後に呼ぶことで描画前にボーンを確定させる)
// ---------------------------------------------------------------------------
void HairPhysicsComponent::ApplySimulationResult()
{
    Actor* ownerActor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(ownerActor != nullptr, L"Object is not Actor");

    for (auto& u : units)
    {
        if (!u.node || !u.hair) continue;

        // PhysXの結果を取得してローカル回転に変換
        PxTransform pxt = u.hair->getGlobalPose();
        Matrix hairWorld = PX_TRANSFORM_TO_MATRIX(pxt);

        if (u.node->parent)
        {
            Matrix parentWorldInv = u.node->parent->worldTransform.Invert();
            Matrix local = hairWorld * parentWorldInv;

            Vector3 s, p;
            Quaternion r;
            local.Decompose(s, r, p);
            u.node->rotation = r;
            // position はボーン長を維持するためアニメ値のままにする
        }
        else
        {
            Vector3 s, p;
            Quaternion r;
            hairWorld.Decompose(s, r, p);
            u.node->rotation = r;
        }
    }

    // 書き戻し後にトランスフォーム再計算
    model->UpdateTransform(ownerActor->transform.matrix);
}

void HairPhysicsComponent::Render(const RenderContext& rc)
{
    // Simulate蠕・ hair蜑帑ｽ薙・邨先棡繧偵・繝ｼ繝ｳ縺ｸ譖ｸ縺肴綾縺・(actors.Render縺悟・縺ｫ蜻ｼ縺ｰ繧後ｋModelRenderer->Render縺ｮ蜑阪↓螳溯｡・
    ApplySimulationResult();

    if (rc.renderSettings.showDebug)
    {
        auto* sr = Game::Graphics::Instance().GetShapeRenderer();
        for (auto& u : units)
        {
            if (!u.hair) continue;

            // hair剛体: 水色の球
            PxTransform hairPose = u.hair->getGlobalPose();
            Matrix hairMat = PX_TRANSFORM_TO_MATRIX(hairPose);
            Vector3 hairPos(hairMat._41, hairMat._42, hairMat._43);
            sr->DrawSphere(hairPos, sphereRadius, Color(0.0f, 0.8f, 1.0f, 1.0f));

            // anchor剛体: 黄色の球(小)
            PxTransform anchorPose = u.anchor->getGlobalPose();
            Matrix anchorMat = PX_TRANSFORM_TO_MATRIX(anchorPose);
            Vector3 anchorPos(anchorMat._41, anchorMat._42, anchorMat._43);
            sr->DrawSphere(anchorPos, sphereRadius * 0.4f, Color(1.0f, 1.0f, 0.0f, 1.0f));

            // anchor -> hair の線
            Game::Graphics::Instance().GetPrimitiveRenderer()->DrawLine(
                anchorPos, hairPos,
                Color(1.0f, 1.0f, 0.0f, 0.8f), Color(0.0f, 0.8f, 1.0f, 0.8f));
        }
    }
}

// ---------------------------------------------------------------------------
// パラメータ調整
// ---------------------------------------------------------------------------
void HairPhysicsComponent::SetSwingLimitDeg(float swing1, float swing2)
{
    swing1Deg = swing1;
    swing2Deg = swing2;
    for (auto& u : units)
        if (u.joint) ApplyJointLimits(u.joint);
}

void HairPhysicsComponent::SetTwistLimitDeg(float lower, float upper)
{
    twistLowerDeg = lower;
    twistUpperDeg = upper;
    for (auto& u : units)
        if (u.joint) ApplyJointLimits(u.joint);
}

void HairPhysicsComponent::SetLinearDamping(float d)
{
    linearDamp = d;
    for (auto& u : units)
        if (u.hair) u.hair->setLinearDamping(d);
}

void HairPhysicsComponent::SetAngularDamping(float d)
{
    angularDamp = d;
    for (auto& u : units)
        if (u.hair) u.hair->setAngularDamping(d);
}

// ---------------------------------------------------------------------------
// DrawGUI
// ---------------------------------------------------------------------------
void HairPhysicsComponent::DrawGUI()
{
    if (ImGui::TreeNode("HairPhysicsComponent (PhysX)"))
    {
        ImGui::Text("Hair units: %d", (int)units.size());

        bool limChanged = false;

        float s1 = swing1Deg, s2 = swing2Deg;
        if (ImGui::DragFloat("Swing1 Limit (deg)", &s1, 0.5f, 0.0f, 90.0f)) { swing1Deg = s1; limChanged = true; }
        if (ImGui::DragFloat("Swing2 Limit (deg)", &s2, 0.5f, 0.0f, 90.0f)) { swing2Deg = s2; limChanged = true; }

        float tl = twistLowerDeg, tu = twistUpperDeg;
        if (ImGui::DragFloat("Twist Lower (deg)", &tl, 0.5f, -180.0f, 0.0f)) { twistLowerDeg = tl; limChanged = true; }
        if (ImGui::DragFloat("Twist Upper (deg)", &tu, 0.5f, 0.0f, 180.0f)) { twistUpperDeg = tu; limChanged = true; }

        if (limChanged)
            for (auto& u : units)
                if (u.joint) ApplyJointLimits(u.joint);

        float ld = linearDamp;
        if (ImGui::DragFloat("Linear Damping", &ld, 0.05f, 0.0f, 20.0f)) SetLinearDamping(ld);

        float ad = angularDamp;
        if (ImGui::DragFloat("Angular Damping", &ad, 0.05f, 0.0f, 20.0f)) SetAngularDamping(ad);

        ImGui::TreePop();
    }
}

// ---------------------------------------------------------------------------
// private: HairUnit構築
// ---------------------------------------------------------------------------
void HairPhysicsComponent::BuildUnit(Model::Node* node, float radius, float mass)
{
    PxPhysics* px = PhysicsManager::Instance().GetPhysics();
    PxMaterial* mat = PhysicsManager::Instance().GetDefaultMaterial();
    PxScene* scene = PhysicsManager::Instance().GetSceneContext().GetScene();

    HairUnit u;
    u.node = node;

    PxTransform initPose = MATRIX_TO_PX_TRANSFORM(node->worldTransform);

    // --- anchor: Kinematic Dynamic ---
    u.anchor = px->createRigidDynamic(initPose);
    u.anchor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
    {
        // PhysX はシェイプなし剛体を警告するためダミーを付ける
        // シミュレーション・クエリ・トリガーすべてオフ
        PxShape* dummy = px->createShape(PxSphereGeometry(0.001f), *mat, true);
        dummy->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
        dummy->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
        dummy->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
        u.anchor->attachShape(*dummy);
        dummy->release();
    }
    scene->addActor(*u.anchor);

    // --- hair: Dynamic (Layer::Body) ---
    u.hair = px->createRigidDynamic(initPose);
    u.hair->setLinearDamping(linearDamp);
    u.hair->setAngularDamping(angularDamp);
    {
        PxShape* s = px->createShape(PxSphereGeometry(radius), *mat, true);
        PhysicsManager::SetLayerToShape(s, Layer::Body);
        u.hair->attachShape(*s);
        s->release();
    }
    PxRigidBodyExt::updateMassAndInertia(*u.hair, mass);
    scene->addActor(*u.hair);

    // --- D6Joint: anchor -> hair ---
    u.joint = PxD6JointCreate(*px,
        u.anchor, PxTransform(PxIdentity),
        u.hair, PxTransform(PxIdentity));

    // 位置拘束: Locked (髪はアンカーから離れない)
    u.joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
    u.joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
    u.joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);

    // 回転拘束: Swing/Twist は角度制限付きで許可
    u.joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
    u.joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
    u.joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);

    ApplyJointLimits(u.joint);

    units.push_back(u);
}

void HairPhysicsComponent::DestroyUnit(HairUnit& u)
{
    PxScene* scene = PhysicsManager::Instance().GetSceneContext().GetScene();
    if (u.joint) { u.joint->release();                          u.joint = nullptr; }
    if (u.hair) { scene->removeActor(*u.hair);   u.hair->release();   u.hair = nullptr; }
    if (u.anchor) { scene->removeActor(*u.anchor); u.anchor->release(); u.anchor = nullptr; }
}

void HairPhysicsComponent::ApplyJointLimits(PxD6Joint* joint)
{
    joint->setTwistLimit(PxJointAngularLimitPair(
        RAD(twistLowerDeg), RAD(twistUpperDeg),
        PxSpring(0.0f, 0.0f)));

    joint->setSwingLimit(PxJointLimitCone(
        RAD(swing1Deg), RAD(swing2Deg),
        PxSpring(0.0f, 0.0f)));
}
