// Object.cpp

#include "Object.h"
#include "imgui.h"
#include "GameTime.h"

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
    if (!isActive) return;

    // Destroy timer
    if (destroyTimer.has_value() && destroyTimer.value() > 0.0f)
        destroyTimer.value() -= Game::Time::deltaTime;

    componentList.Update();
    OnUpdate();

    OnLateUpdate();
    componentList.LateUpdate();
}

void Object::Render(const RenderContext& rc)
{
    if (!isActive) return;

    componentList.Render(rc);
    OnRender(rc);
}

void Object::DrawGUI()
{
    if (ImGui::TreeNode("Object Info"))
    {
        ImGui::Text("Name: %s", name.c_str());
        ImGui::Checkbox("Active", &isActive);
        ImGui::Text("Components: %d", (int)componentList.data.size());
        if (ImGui::Button("Destroy"))
            Destroy();
        ImGui::Text("Destroy Timer: %s", destroyTimer.has_value() ? std::to_string(destroyTimer.value()).c_str() : "N/A");
        ImGui::TreePop();
    }
}

// Components

void Object::Components::push_back(std::unique_ptr<Component> component)
{
    data.push_back(std::move(component));
}

void Object::Components::Update()
{
    for (auto& c : data)
        if (c->IsActive()) c->Update();
}

void Object::Components::LateUpdate()
{
    for (auto& c : data)
        if (c->IsActive()) c->LateUpdate();
}

void Object::Components::Render(const RenderContext& rc)
{
    for (auto& c : data)
        if (c->IsActive()) c->Render(rc);
}