// Object.cpp

#include "Object.h"
#include "imgui.h"

void Object::Components::DrawGUI()
{
    for (auto& c : data) {
        ImGui::PushID((void*)((uintptr_t)c.get() ^ (uintptr_t)this));
        c->DrawGUI();
        ImGui::PopID();
    }
}

void Object::Update()
{
    componentList.Update();
    OnUpdate();

    componentList.LateUpdate();
    OnLateUpdate();
}

void Object::Render(const RenderContext& rc)
{
    componentList.Render(rc);
    OnRender(rc);
}

void Object::DrawGUI()
{
    ImGui::PushID(this);
    if (ImGui::CollapsingHeader(name.empty() ? "Unnamed Object" : name.c_str()))
    {
        componentList.DrawGUI();

        if (ImGui::TreeNode("User param"))
        {
            OnDrawGUI();
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

void Object::Components::Update()
{
    for (auto& c : data) {
        c->Update();
    }
}

void Object::Components::LateUpdate()
{
    for (auto& c : data) {
        c->LateUpdate();
    }
}

void Object::Components::Render(const RenderContext& rc)
{
    for (auto& c : data) {
        c->Render(rc);
    }
}