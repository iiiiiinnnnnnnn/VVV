// HairPhysicsComponent.cpp

#include "HairPhysicsComponent.h"
#include "Actor.h"
#include "Graphics.h"

// ---------------------------------------------------------------------------
// コンストラクタ
// ---------------------------------------------------------------------------
HairPhysicsComponent::HairPhysicsComponent(
    Object* owner,
    Model* model,
    const std::vector<std::string>& boneNames,
    float sphereRadius,
    float mass)
    : Component(owner)
    , model(model)
    , sphereRadius(sphereRadius)
{

}

HairPhysicsComponent::~HairPhysicsComponent()
{

}

void HairPhysicsComponent::LateUpdate()
{

}

void HairPhysicsComponent::Render(const RenderContext& rc)
{

}

void HairPhysicsComponent::DrawGUI()
{
    if (ImGui::TreeNode("HairPhysicsComponent"))
    {

        ImGui::TreePop();
    }
}
