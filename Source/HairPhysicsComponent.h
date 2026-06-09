// HairPhysicsComponent.h

#pragma once

#include "Component.h"
#include "Model.h"
#include "PhysicsManager.h"

class HairPhysicsComponent : public Component
{
public:
    HairPhysicsComponent(Object* owner, Model* model,
        const std::vector<std::string>& boneNames,
        float sphereRadius = 0.04f,
        float mass = 0.1f);

    ~HairPhysicsComponent() override;

    void LateUpdate() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

private:
    struct HairUnit
    {
        Model::Node* node = nullptr;
        PxRigidDynamic* anchor = nullptr;  // Kinematic
        PxRigidDynamic* hair = nullptr;  // Dynamic
        PxD6Joint* joint = nullptr;
    };

    Model* model = nullptr;
    std::vector<HairUnit> units;
	float sphereRadius;
};
