// HairPhysicsComponent.h

#pragma once

#include "Component.h"
#include "Model.h"
#include <vector>
#include <string>

class HairPhysicsComponent : public Component
{
public:
    HairPhysicsComponent(Object* owner, Model* model,
        const std::vector<std::string>& boneNames,
        float stiffness = 50.0f,
        float damping = 5.0f,
        float gravity = 9.8f);

    void LateUpdate() override;
    void DrawGUI() override;

    struct BodySphere
    {
        Model::Node* node = nullptr;
        Matrix       offset = Matrix::Identity;
        float        radius = 0.1f;
    };

    void AddBodySphere(int nodeIndex, float radius, Matrix offset = Matrix::Identity);

private:
    struct HairBone
    {
        Model::Node* node = nullptr;
        Quaternion   bindRot = Quaternion::Identity; // バインドポーズの回転
        Vector3      velocity = Vector3::Zero;        // 先端位置の速度
        Vector3      currentPos = Vector3::Zero;        // シミュレーション上の先端位置
    };

    std::vector<BodySphere> bodySpheres;

    Model* model = nullptr;
    std::vector<HairBone> bones;
    float stiffness; // バネ定数（元に戻る力）
    float damping;   // 減衰
    float gravity;   // 重力
};