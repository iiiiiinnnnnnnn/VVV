// Object.cpp

#include "Core/Object/Object.h"
#include "imgui.h"
#include "Application/Time/GameTime.h"

Object::~Object()
{
	while (!components.empty())
	{
		components.pop_back();
	}
}

void Object::Awake()
{
	if (isAwake) return;

	isAwake = true;
    SortComponentsByUpdateOrder();

    OnAwake();

    for (size_t i = 0; i < components.size(); ++i)
    {
		Component* component = components[i].get();
		if (component->isAwake) continue;

        component->Awake();
		component->isAwake = true;
    }
}

void Object::Start()
{
	if (isStarted || !isAwake || !isActive) return;

	isStarted = true;

    OnStart();

    for (size_t i = 0; i < components.size(); ++i)
    {
		Component* component = components[i].get();
		if (!component->IsActive() || component->isStarted) continue;

        component->Start();
		component->isStarted = true;
    }
}

void Object::Update()
{
    if (!isActive) return;
	if (!isAwake) Awake();
	if (!isStarted) Start();

    if (destroyTimer.has_value() && destroyTimer.value() > 0.0f)
        destroyTimer.value() -= Game::Time::deltaTime;

    OnUpdate();

    for (auto& c : components)
    {
        if (c->IsActive())
            c->Update();
    }
}

void Object::LateUpdate()
{
    if (!isActive) return;

    OnLateUpdate();

    for (auto& c : components)
    {
        if(c->IsActive())
            c->LateUpdate();
    }
}

void Object::Render(const RenderContext& rc)
{
    if (!isActive) return;

    for (auto& c : components)
    {
        if(c->IsActive())
            c->Render(rc);
    }

    OnRender(rc);
}

void Object::DrawGUI()
{
    if (ImGui::TreeNode("Object Info"))
    {
        ImGui::Checkbox("Active", &isActive);
        ImGui::Text("Name: %s, Tag: %s", name.c_str(), tag.c_str());
        ImGui::Text("Destroy Timer: %s", destroyTimer.has_value() ? std::to_string(destroyTimer.value()).c_str() : "N/A");

        if (ImGui::Button("Destroy"))
            Destroy();

        ImGui::TreePop();
    }

    for (auto& c : components)
    {
        ImGui::PushID((void*)((uintptr_t)c.get() ^ (uintptr_t)this));
        if (ImGui::TreeNode(c->GetDebugName()))
        {
            c->DrawGUI();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void Object::SortComponentsByUpdateOrder()
{
    if (!updateOrderDirty) return;

    std::stable_sort(
        components.begin(),
        components.end(),
        [](const std::unique_ptr<Component>& a, const std::unique_ptr<Component>& b)
    {
        return a->GetUpdateOrder() < b->GetUpdateOrder();
    });

    updateOrderDirty = false;
}
