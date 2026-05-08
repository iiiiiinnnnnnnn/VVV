// Animator.cpp

#include "Animator.h"

Animator::Animator(Actor* owner, std::shared_ptr<Model> model) : Component(owner), model(model) {
}

void Animator::Update(float elapsedTime)
{
    if (!model || !playing || currentAnimationIndex < 0)
        return;

    const Model::Animation& anim = model->GetAnimations()[currentAnimationIndex];

    model->ComputeAnimation(currentAnimationIndex, currentTime, nodePoses);
    model->SetNodePoses(nodePoses);
    model->UpdateTransform(Matrix::Identity);

    currentTime += elapsedTime * speed;

    if (currentTime > anim.secondsLength)
    {
        if (loop)
            currentTime -= anim.secondsLength;
        else
            currentTime = anim.secondsLength;
    }
}

void Animator::DrawGUI(float elapsedTime)
{
    if (ImGui::CollapsingHeader("Animator"))
    {
        ImGui::Checkbox("Loop", &loop); ImGui::SameLine();
        ImGui::SetNextItemWidth(70);

        if (model != nullptr)
        {
            float secondsLength = currentAnimationIndex >= 0 ? model->GetAnimations().at(currentAnimationIndex).secondsLength : 0;
            int currentFrame = static_cast<int>(currentTime * 60.0f);
            int frameLength = static_cast<int>(secondsLength * 60);

            ImGui::SetNextItemWidth(50);
            ImGui::PushID(u8"フレーム");
            if (ImGui::DragInt("##v", &currentFrame, 1, 0, frameLength))
            {
                playing = true;
                currentTime = currentFrame / 60.0f;
                speed = 0.0f;
            }
            ImGui::PopID();

            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::PushID(u8"タイムライン");
            if (ImGui::SliderFloat("##v", &currentTime, 0, secondsLength, "%.3f"))
            {
                playing = true;
                speed = 0.0f;
            }
            ImGui::PopID();

            int index = 0;
            for (const Model::Animation& animation : model->GetAnimations())
            {
                ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf;

                ImGui::TreeNodeEx(&animation, nodeFlags, "[%d] %s", index, animation.name.c_str());

                // ダブルクリックでアニメーション再生
                if (ImGui::IsItemClicked())
                {
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        playing = true;
                        currentAnimationIndex = index;
                        currentTime = 0.0f;
                        speed = 1.0f;
                    }
                }

                ImGui::TreePop();

                index++;
            }
        }
    }
}

void Animator::Play(int index, bool loop)
{
    if (!model) return;

    currentAnimationIndex = index;
    currentTime = 0.0f;
    playing = true;
    this->loop = loop;
}

void Animator::Stop() {
    playing = false;
}