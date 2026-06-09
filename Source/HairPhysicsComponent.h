// HairPhysicsComponent.h
#pragma once

#include "Component.h"
#include "Model.h"

// SpringBone 方式の髪物理コンポーネント
// - PhysX を使わず、自前のバネ積分で動く
// - ボーン親子関係を維持
// - 体貫通防止用の静止球コライダーを複数登録可能
// - MMD 風「物理で揺れつつ元に戻ろうとする」挙動

class HairPhysicsComponent : public Component
{
public:
    // ---- 体貫通防止用の静止剛体 ----
    struct BodySphere
    {
        std::string boneName;   // 追従させるボーン名（空なら固定）
        Vector3     offset;     // ボーンローカルオフセット
        float       radius;
    };

    struct InnerSphere
    {
        std::string boneName;
        Vector3     offset;
        float       radius;  // この半径の内側にしか入れない
    };

    // bodySpheresは空でも構わない
    HairPhysicsComponent(
        Object* owner,
        Model* model,
        const std::vector<std::string>& boneNames,
        const std::vector<BodySphere>& bodySpheres = {},
        const std::vector<InnerSphere>& innerSpheres = {},
        float boneRadius   = 0.04f,
        float stiffness    = 40.0f,
        float damping      = 2.0f,
        float gravity      = 9.8f);

    ~HairPhysicsComponent() override = default;

    void LateUpdate() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

private:
    // バネシミュレーション用のノード1つ分
    struct SpringNode
    {
        Model::Node* node       = nullptr;
        int          nodeIndex  = -1;

        // ワールド空間の「現在の物理位置」
        Vector3 position;
        Vector3 velocity;

        // このノードがチェーンの中で何番目か（0=ルート）
        int chainDepth = 0;
    };

    // 1本の髪チェーン（連続した親子ボーン）
    struct HairChain
    {
        std::vector<SpringNode> nodes; // [0]=ルート … [N]=先端
    };

    void BuildChains(const std::vector<std::string>& boneNames);
    void ResetPhysics();
    void StepSpring(float dt);
    void PushOutBodySpheres(SpringNode& node);
    void PullInInnerSpheres(SpringNode& sn);
    Vector3 GetBodySphereWorldPos(const BodySphere& bs) const;

    Model*  model      = nullptr;
    Matrix  ownerWorld = Matrix::Identity; // 前フレームのオーナーワールド行列

    std::vector<HairChain>  chains;
    std::vector<BodySphere> bodySpheres;
    std::vector<InnerSphere> innerSpheres;
    int  guiSelectedInner = -1;
    char guiInnerBoneName[64] = {};

    float boneRadius;
    float stiffness;   // バネ強さ（元に戻る力）
    float damping;     // 減衰
    float gravity;

    // ImGui 用: BodySphere 編集
    int   guiSelectedSphere = -1;
    char  guiBoneName[64]   = {};
};