#include "Gameplay/Actor/Actor.h"

#include "Physics/RigidBody/Rigidbody.h"
#include "Physics/Collider/CharacterController.h"
#include "Physics/Core/PhysicsComponent.h"
#include <imgui.h>

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
    transform.Update();

    Object::Update();
}

void Actor::DrawGUI()
{
	DrawGUI(false);
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
    const bool inspectorOpen = ImGui::CollapsingHeader(name.empty() ? (const char*)u8"名前なしオブジェクト" : name.c_str(), flags);
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
    if (inspectorOpen)
    {
        Transform::TransformChangedResult res = transform.DrawGUI();
        if (res.positionChanged)
        {
            auto rb = GetComponent<Rigidbody>();
            if (rb)
            {
                RigidbodyDynamic* rbd = dynamic_cast<RigidbodyDynamic*>(rb);
                if (rbd)
                {
                    rbd->SetVelocity(Vector3::Zero);
                }
                rb->SetPosition(transform.position);
            }

            auto cc = GetComponent<CharacterController>();
            if (cc)
                cc->SetPosition(transform.position);
        }
        if (res.rotationChanged)
        {
            auto rb = GetComponent<Rigidbody>();
            if (rb)
                rb->SetRotation(transform.rotation);
        }
        if (res.scaleChanged)
        {
            // you have no idea what's come
        }

        Object::DrawGUI();

        if (ImGui::TreeNode((const char*)u8"ユーザーパラメーター"))
        {
            OnDrawGUI();
            ImGui::TreePop();
        }
    }
    ImGui::PopID();
}
