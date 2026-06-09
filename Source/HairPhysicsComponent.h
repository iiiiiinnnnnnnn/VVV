// HairPhysicsComponent.h

#pragma once

#include "Component.h"
#include "Model.h"
#include "PhysicsManager.h"
#include <vector>
#include <string>

// MMD式「位置+物理」髪剛体コンポーネント
//
// フレームの流れ:
//   LateUpdate() ... anchorをアニメ後ボーン位置にsetKinematicTarget
//   PhysicsManager::Simulate() ... PhysXがhair剛体を計算
//   Render()     ... hair剛体の結果をボーンに書き戻し → model->UpdateTransform()
//
// 各髪ボーンにつき以下を生成:
//   anchor : Kinematic Dynamic (ボーン追従、シェイプなし相当)
//   hair   : Dynamic           (Layer::Body, 球コライダー)
//   joint  : PxD6Joint         (位置拘束=Locked, Swing/Twist=Limited)

class HairPhysicsComponent : public Component
{
public:
    HairPhysicsComponent(Object* owner, Model* model,
        const std::vector<std::string>& boneNames,
        float sphereRadius = 0.04f,
        float mass = 0.1f);

    ~HairPhysicsComponent() override;

    // Simulate前: anchorをボーン位置に追従
    void LateUpdate() override;

    // Simulate後: hair剛体の結果をボーンね書き戻す (Scene::Updateから呼ぶ)
    void ApplySimulationResult();

    // Simulate後: hair剛体の結果をボーンに書き戻してUpdateTransform
    // Render()の先頭で呼ぶことでSimulate確実後に処理される
    void Render(const RenderContext& rc) override;

    void DrawGUI() override;

    // 生成後でも変更可能なパラメータ
    void SetSwingLimitDeg(float swing1Deg, float swing2Deg);
    void SetTwistLimitDeg(float lowerDeg, float upperDeg);
    void SetLinearDamping(float d);
    void SetAngularDamping(float d);

private:
    struct HairUnit
    {
        Model::Node* node = nullptr;
        PxRigidDynamic* anchor = nullptr;  // Kinematic
        PxRigidDynamic* hair = nullptr;  // Dynamic
        PxD6Joint* joint = nullptr;
    };

    void BuildUnit(Model::Node* node, float radius, float mass);
    void DestroyUnit(HairUnit& u);
    void ApplyJointLimits(PxD6Joint* joint);

    Model* model = nullptr;
    std::vector<HairUnit> units;

    float sphereRadius = 0.04f;
    float linearDamp = 0.5f;
    float angularDamp = 5.0f;
    float swing1Deg = 20.0f;
    float swing2Deg = 20.0f;
    float twistLowerDeg = -5.0f;
    float twistUpperDeg = 5.0f;
};
