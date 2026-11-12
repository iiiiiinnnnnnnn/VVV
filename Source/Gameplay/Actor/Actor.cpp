#include <ProfileKit/ProfileKit.h>
#include <ImGuiToolKit/Widgets.h>
#include "Gameplay/Actor/Actor.h"
#include "IconsFontAwesome5.h"
#include "Gameplay/Component/ActorHitstopComponent.h"

#include "Physics/RigidBody/Rigidbody.h"
#include "Physics/Collider/CharacterController.h"
#include "Physics/Core/PhysicsComponent.h"
#include <imgui.h>

Actor::~Actor()
{
	Uninit();
}

void Actor::Uninit()
{
	PROFILE_FUNCTION();
	if (uninitialized) return;
	uninitialized = true;
	if (components.empty()) Object::Destroy();
	else Destroy();
	if (initialized) OnUninit();
	initialized = false;
	isActive = false;
	while (!components.empty()) components.pop_back();
}

void Actor::Awake()
{
	PROFILE_FUNCTION();
	if (initialized || uninitialized) return;
	initialized = true;
	try
	{
		Object::Awake();
	}
	catch (...)
	{
		Uninit();
		throw;
	}
}

void Actor::Destroy(float delay)
{
    Object::Destroy(delay);
	CollisionEventCallback& eventCallback =
		PhysicsManager::Instance().GetSceneContext().GetEventCallback();

	for (const auto& component : components)
	{
		if (PhysicsComponent* collider = dynamic_cast<PhysicsComponent*>(component.get()))
		{
			eventCallback.RemoveCollider(collider);
			CCHitReport::RemoveColliderFromAll(collider);
			collider->SetActive(false);
		}
		if (CharacterController* controller = dynamic_cast<CharacterController*>(component.get()))
			controller->ReleaseController();
		if (auto* rigidbody = dynamic_cast<Rigidbody*>(component.get()))
        {
            rigidbody->SetSceneEnabled(false);
        }
    }
}
void Actor::Update()
{
	PROFILE_FUNCTION();
	if (!IsActive()) return;
	hitstop = GetComponent<ActorHitstopComponent>();
	hitstopThisFrame = hitstop && hitstop->IsStopping();
	Object::Update();
}

bool Actor::IsUpdateSuspended() const
{
	return hitstopThisFrame || (hitstop && hitstop->IsStopping());
}

void Actor::DrawGUI()
{
	ImGui::PushID(this);
	DrawBasicInspector();
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	const auto changed = transform.DrawGUI();
	if (changed.positionChanged)
	{
		if (auto* rigidbody = GetComponent<Rigidbody>())
		{
			if (auto* dynamicBody = dynamic_cast<RigidbodyDynamic*>(rigidbody))
				dynamicBody->SetVelocity(Vector3::Zero);
			rigidbody->SetPosition(transform.position);
		}
		if (auto* controller = GetComponent<CharacterController>()) controller->SetPosition(transform.position);
	}
	if (changed.rotationChanged)
		if (auto* rigidbody = GetComponent<Rigidbody>()) rigidbody->SetRotation(transform.rotation);

	if (ImGuiToolKit::CollapsingHeader(u8"ユーザー定義", ImGuiTreeNodeFlags_DefaultOpen)) OnDrawGUI();
	DrawComponentInspector();
	ImGui::PopID();
}

void Actor::DrawGUI(bool selected)
{
    ImGui::PushID(this);
	const ImGuiTreeNodeFlags flags = selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
	if (selected) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (selected)
	{
		const ImVec4 selectedColor = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
		ImGui::PushStyleColor(ImGuiCol_Header, selectedColor);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, selectedColor);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, selectedColor);
	}
	const std::string label = std::string(ICON_FA_CUBE " ") +
		(name.empty() ? JText(u8"名前なしオブジェクト") : name) + "###ActorInspector";
    const bool inspectorOpen = ImGui::CollapsingHeader(label.c_str(), flags);
	if (selected)
	{
		const ImVec2 itemMin = ImGui::GetItemRectMin();
		const ImVec2 itemMax = ImGui::GetItemRectMax();
		const ImU32 accentColor = ImGui::GetColorU32(ImGuiCol_NavCursor);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRect(itemMin, itemMax, accentColor, 2.0f, 0, 2.0f);
		drawList->AddRectFilled(itemMin, ImVec2(itemMin.x + 4.0f, itemMax.y), accentColor);
		ImGui::PopStyleColor(3);
	}
	if (inspectorOpen) DrawGUI();
	ImGui::PopID();
}
