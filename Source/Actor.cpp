// Actor.cpp

#include "Actor.h"
#include "imgui.h"

void Actor::Components::push_back(std::unique_ptr<Component> component)
{
    data.push_back(std::move(component));
}

void Actor::Components::Update(float elapsedTime)
{
    for (auto& c : data) {
        c->Update(elapsedTime);
    }
}

void Actor::Components::Render(const RenderContext& rc, float elapsedTime)
{
    for (auto& c : data) {
        c->Render(rc, elapsedTime);
    }
}

void Actor::Components::DrawGUI(float elapsedTime)
{
    for (auto& c : data) {
        ImGui::PushID((void*)((uintptr_t)c.get() ^ (uintptr_t)this));
        c->DrawGUI(elapsedTime);
        ImGui::PopID();
    }
}

void Actor::Update(float elapsedTime)
{
    transform.Update();
    componentList.Update(elapsedTime);
    OnUpdate(elapsedTime);
}

void Actor::Render(const RenderContext& rc, float elapsedTime)
{
    componentList.Render(rc, elapsedTime);
    OnRender(rc, elapsedTime);
}

void Actor::DrawGUI(float elapsedTime)
{
    ImGui::PushID(this);
    if (ImGui::CollapsingHeader(name.empty() ? "Unnamed Actor" : name.c_str()))
    {
        if (ImGui::TreeNode("Transform"))
        {
            ImGui::InputFloat3("Position", &transform.position.x);
            ImGui::InputFloat4("Rotation", &transform.rotation.x);
            ImGui::InputFloat3("Scale", &transform.scale.x);
            ImGui::TreePop();
        }
        componentList.DrawGUI(elapsedTime);
        if (ImGui::TreeNode("User param"))
        {
            OnDrawGUI(elapsedTime);
            ImGui::TreePop();
        }
    }
    ImGui::PopID();

    ImGui::Separator();
}
