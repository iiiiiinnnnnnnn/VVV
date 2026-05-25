// Object.cpp

#include "Object.h"
#include "imgui.h"

void Object::Components::DrawGUI(float elapsedTime)
{
    for (auto& c : data) {
        ImGui::PushID((void*)((uintptr_t)c.get() ^ (uintptr_t)this));
        c->DrawGUI(elapsedTime);
        ImGui::PopID();
    }
}

void Object::Update(float elapsedTime)
{
    componentList.Update(elapsedTime);
    OnUpdate(elapsedTime);

    componentList.LateUpdate(elapsedTime);
    OnLateUpdate(elapsedTime);
}

void Object::Render(const RenderContext& rc, float elapsedTime)
{
    componentList.Render(rc, elapsedTime);
    OnRender(rc, elapsedTime);
}

void Object::DrawGUI(float elapsedTime)
{
    ImGui::PushID(this);
    if (ImGui::CollapsingHeader(name.empty() ? "Unnamed Object" : name.c_str()))
    {
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

// Components

void Object::Components::push_back(std::unique_ptr<Component> component)
{
    data.push_back(std::move(component));
}

void Object::Components::Update(float elapsedTime)
{
    for (auto& c : data) {
        c->Update(elapsedTime);
    }
}

void Object::Components::LateUpdate(float elapsedTime)
{
    for (auto& c : data) {
        c->LateUpdate(elapsedTime);
    }
}

void Object::Components::Render(const RenderContext& rc, float elapsedTime)
{
    for (auto& c : data) {
        c->Render(rc, elapsedTime);
    }
}