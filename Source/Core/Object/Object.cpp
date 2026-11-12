#include <ProfileKit/ProfileKit.h>
#include <ImGuiToolKit/Widgets.h>
#include "Core/Object/Object.h"
#include "IconsFontAwesome5.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "Application/Time/GameTime.h"
#include "Rendering/Core/RenderContext.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Gameplay/Stage/Component/Terrain.h"

Object::~Object()
{
	while (!components.empty())
	{
		components.pop_back();
	}
}

void Object::Awake()
{
	if (isAwake || isAwaking) return;
	isAwaking = true;
	try
	{
		OnAwake();
		// OnAwakeで追加されたコンポーネントも、構成が揃ってから初期化する。
		for (size_t i = 0; i < components.size(); ++i) components[i]->Awake();
		SortComponentsByUpdateOrder();
	}
	catch (...)
	{
		isAwaking = false;
		throw;
	}
	isAwaking = false;
	isAwake = true;
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
    }
}

void Object::Update()
{
    PROFILE_FUNCTION();
    if (!isActive) return;
	PROFILE_SCOPE(name.c_str());
	if (!isAwake) Awake();
	if (!isStarted) Start();

    if (destroyTimer && !IsUpdateSuspended()) destroyTimer->Update();

    if (!IsUpdateSuspended())
		OnUpdate();

    for (auto& c : components)
    {
        if (c->IsActive() && (!IsUpdateSuspended() || c->RunsWhenOwnerSuspended()))
        {
            PROFILE_SCOPE(c->GetDebugName());
            c->Update();
        }
    }
}

void Object::LateUpdate()
{
    PROFILE_FUNCTION();
    if (!isActive) return;
	PROFILE_SCOPE(name.c_str());

    if (!IsUpdateSuspended())
		OnLateUpdate();

    for (auto& c : components)
    {
        if (c->IsActive() && (!IsUpdateSuspended() || c->RunsWhenOwnerSuspended()))
        {
            PROFILE_SCOPE(c->GetDebugName());
            c->LateUpdate();
        }
    }
}

void Object::Render(const RenderContext& rc)
{
    PROFILE_FUNCTION();
    if (!isActive) return;
	PROFILE_SCOPE(name.c_str());

    for (auto& c : components)
    {
		if(c->IsActive())
		{
			PROFILE_SCOPE(c->GetDebugName());
			const bool isCollider = dynamic_cast<PhysicsComponent*>(c.get()) != nullptr;
			c->SetShowDebug(
				rc.renderSettings.showDebug &&
				(isCollider ? rc.renderSettings.showColliderDebug : rc.renderSettings.showComponentDebug));
            c->Render(rc);
		}
    }

    OnRender(rc);
}

void Object::DrawGUI()
{
    if (ImGuiToolKit::TreeNode(ICON_FA_INFO_CIRCLE " " u8"オブジェクト情報"))
    {
        ImGuiToolKit::Checkbox(u8"有効", &isActive);
        ImGuiToolKit::Text(u8"名前: %s、タグ: %s", name.c_str(), tag.c_str());
        ImGuiToolKit::Text(u8"破棄タイマー: %s", destroyTimer.has_value() ? std::to_string(destroyTimer->GetRemaining()).c_str() : JText(u8"なし"));

        if (ImGuiToolKit::Button(u8"破棄"))
            Destroy();

        ImGui::TreePop();
    }

    for (auto& c : components)
    {
		if (dynamic_cast<Terrain*>(c.get())) continue;
        ImGui::PushID((void*)((uintptr_t)c.get() ^ (uintptr_t)this));
        if (ImGui::TreeNode(c->GetDebugName()))
        {
            c->DrawGUI();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void Object::DrawBasicInspector()
{
	ImGui::AlignTextToFramePadding();
	ImGuiToolKit::TextUnformatted(u8"名前：");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##ObjectName", &name);
	ImGui::AlignTextToFramePadding();
	ImGuiToolKit::TextUnformatted(u8"タグ：");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##ObjectTag", &tag);
	ImGuiToolKit::Checkbox(u8"有効", &isActive);
}

void Object::DrawComponentInspector()
{
	ImGuiToolKit::SeparatorText(u8"コンポーネント");
	const float clearWidth = ImGui::CalcTextSize(ICON_FA_TIMES).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::SetNextItemWidth(-clearWidth - ImGui::GetStyle().ItemSpacing.x);
	const bool searchChanged = ImGui::InputTextWithHint("##ComponentSearch",
		JText(u8"コンポーネント名で検索…"), &componentSearch);
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_TIMES "##ClearComponentSearch")) componentSearch.clear();
	if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"検索をクリア");

	const ImGuiTextFilter filter(componentSearch.c_str());
	bool hasMatches = false;
	for (auto& component : components)
	{
		if (dynamic_cast<Terrain*>(component.get())) continue;
		if (!filter.PassFilter(component->GetDebugName())) continue;
		hasMatches = true;
		ImGui::PushID(component.get());
		if (searchChanged && filter.IsActive()) ImGui::SetNextItemOpen(true);
		if (ImGui::CollapsingHeader(component->GetDebugName()))
		{
			bool active = component->IsActive();
			if (ImGuiToolKit::Checkbox(u8"有効##Component", &active)) component->SetActive(active);
			component->DrawGUI();
		}
		ImGui::PopID();
	}
	if (!hasMatches) ImGuiToolKit::TextDisabled(u8"表示するコンポーネントがありません");
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
