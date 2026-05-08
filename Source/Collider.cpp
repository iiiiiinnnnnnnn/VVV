#include "Collider.h"

Collider::Collider(Actor* owner) : Component(owner) {
}

void Collider::Update(float elapsedTime)
{
}

void Collider::DrawGUI(float elapsedTime)
{
}

BoxCollider::BoxCollider(Actor* owner, const Vector3& size) : Collider(owner), size(size) {

}

void BoxCollider::Update(float elapsedTime)
{
}

void BoxCollider::DrawGUI(float elapsedTime)
{
    if (ImGui::CollapsingHeader("BoxCollider"))
    {
        ImGui::InputFloat3("Size", &size.x);
    }
}

CapsuleCollider::CapsuleCollider(Actor* owner, float radius, float height) : Collider(owner), radius(radius), height(height)
{
}

void CapsuleCollider::Update(float elapsedTime)
{
}

void CapsuleCollider::DrawGUI(float elapsedTime)
{
    if (ImGui::CollapsingHeader("CapsuleCollider"))
    {
        ImGui::InputFloat("Radius", &radius);
        ImGui::InputFloat("Height", &height);
	}
}

SphereCollider::SphereCollider(Actor* owner, float radius) : Collider(owner), radius(radius)
{
}

void SphereCollider::Update(float elapsedTime)
{
}

void SphereCollider::DrawGUI(float elapsedTime)
{
}
