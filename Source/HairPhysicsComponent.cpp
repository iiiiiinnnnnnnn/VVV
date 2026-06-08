// HairPhysicsComponent.cpp

#include "HairPhysicsComponent.h"
#include "GameTime.h"
#include "Actor.h"

HairPhysicsComponent::HairPhysicsComponent(
    Object* owner, Model* model,
    const std::vector<std::string>& boneNames,
    float stiffness, float damping, float gravity)
    : Component(owner)
    , model(model)
    , stiffness(stiffness)
    , damping(damping)
    , gravity(gravity)
{
    for (auto& node : model->GetNodes())
    {
        for (const auto& name : boneNames)
        {
            if (node.name == name)
            {
                HairBone bone;
                bone.node = const_cast<Model::Node*>(&node);
                bone.bindRot = node.rotation;
                bone.currentPos = { node.worldTransform._41,
                    node.worldTransform._42,
                    node.worldTransform._43 };
                bones.push_back(bone);
            }
        }
    }
}

void HairPhysicsComponent::LateUpdate()
{
    float dt = Game::Time::deltaTime;

    for (auto& bone : bones)
    {
        // バインドポーズの先端位置（目標）
        Quaternion bindWorld = bone.bindRot;
        if (bone.node->parent)
            bindWorld = bone.node->parent->rotation * bone.bindRot;

        Vector3 targetPos = {
            bone.node->worldTransform._41,
            bone.node->worldTransform._42,
            bone.node->worldTransform._43
        };

        // バネ力（目標位置に戻ろうとする力）
        Vector3 springForce = (targetPos - bone.currentPos) * stiffness;

        // 重力
        Vector3 gravityForce = { 0, -gravity, 0 };

        // 速度更新
        bone.velocity += (springForce + gravityForce) * dt;
        bone.velocity *= (1.0f - damping * dt); // 減衰

        // 位置更新
        bone.currentPos += bone.velocity * dt;

        // currentPosからboneのrotationに反映
        Vector3 dir = bone.currentPos - targetPos;
        if (dir.LengthSquared() > 0.0001f)
        {
            dir.Normalize();
            Vector3 bindDir = { bone.node->worldTransform._21,
                bone.node->worldTransform._22,
                bone.node->worldTransform._23 };
            bindDir.Normalize();
            bone.node->rotation = Quaternion::FromToRotation(bindDir, dir) * bone.bindRot;
        }
    }

    // 身体剛体との貫通防止（押し出し）
    for (auto& bone : bones)
    {
        for (auto& body : bodySpheres)
        {
            // 身体剛体のワールド位置
            Matrix bodyWorld = body.offset * body.node->worldTransform;
            Vector3 bodyPos = { bodyWorld._41, bodyWorld._42, bodyWorld._43 };

            Vector3 diff = bone.currentPos - bodyPos;
            float dist = diff.Length();
            float minDist = body.radius; // 髪の点は半径0扱い

            if (dist < minDist && dist > 0.0001f)
            {
                // 押し出す
                diff.Normalize();
                bone.currentPos = bodyPos + diff * minDist;
                // 速度も押し出し方向に補正
                float vDot = bone.velocity.Dot(diff);
                if (vDot < 0)
                    bone.velocity -= diff * vDot;
            }
        }
    }

    Actor* ownerActor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(ownerActor != nullptr, L"Object is not Actor");

    // 回転を反映するために再度UpdateTransform
    model->UpdateTransform(
        ownerActor->transform.matrix
    );
}

void HairPhysicsComponent::DrawGUI()
{
    if (ImGui::TreeNode("HairPhysicsComponent"))
    {
        ImGui::DragFloat("Stiffness", &stiffness, 1.0f, 0.0f, 200.0f);
        ImGui::DragFloat("Damping", &damping, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("Gravity", &gravity, 0.1f, 0.0f, 20.0f);

        ImGui::Separator();
        ImGui::Text("Body Spheres (%d)", (int)bodySpheres.size());

        for (int i = 0; i < (int)bodySpheres.size(); ++i)
        {
            auto& s = bodySpheres[i];
            ImGui::PushID(i);

            // ノード名表示
            std::string nodeName = s.node ? s.node->name : "None";
            ImGui::Text("[%d] %s", i, nodeName.c_str());
            ImGui::SameLine();
            if (ImGui::Button("x"))
            {
                bodySpheres.erase(bodySpheres.begin() + i);
                ImGui::PopID();
                break;
            }

            ImGui::DragFloat("Radius", &s.radius, 0.001f, 0.0f, 1.0f);

            ImGui::PopID();
        }

        ImGui::Separator();

        // 追加UI
        static int addNodeIndex = 0;
        static float addRadius = 0.1f;
        ImGui::InputInt("Node Index", &addNodeIndex);
        ImGui::DragFloat("Add Radius", &addRadius, 0.001f, 0.0f, 1.0f);
        if (ImGui::Button("Add Body Sphere"))
        {
            auto& nodes = model->GetNodes();
            if (addNodeIndex >= 0 && addNodeIndex < (int)nodes.size())
                AddBodySphere(addNodeIndex, addRadius);
        }

        ImGui::TreePop();
    }
}

void HairPhysicsComponent::AddBodySphere(int nodeIndex, float radius, Matrix offset)
{
    BodySphere s;
    s.node = &model->GetNodes()[nodeIndex];
    s.radius = radius;
    s.offset = offset;
    bodySpheres.push_back(s);
}