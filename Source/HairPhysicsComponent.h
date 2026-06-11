// HairPhysicsComponent.h
#pragma once

#include "Component.h"
#include "Model.h"

class HairPhysicsComponent : public Component
{
public:
    struct Bone
    {
        int nodeIndex = -1;

        Vector3 localPosition;
        Quaternion localRotation;

        Matrix worldTransform;
        Vector3 oldWorldPosition;
    };

    struct SpringCapsule
    {
        Vector3 start;
        Vector3 end;
        float   radius;
        int     nodeIndex;
    };

    // bodySpheresÇÕãÛÇ≈Ç‡ç\ÇÌÇ»Ç¢
    HairPhysicsComponent(
        Object* owner,
        Model* model,
        std::vector<std::string> boneContainNames,
        std::vector<SpringCapsule> bodycapsules = {});

    ~HairPhysicsComponent() override = default;

    void LateUpdate() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

private:
    std::vector<Bone>					bones;
    Vector3								gravity = { 0, -0.3f, 0 };
    float 								damping = 0.98f;
    float								maxVelocity = 0.3f;
    bool hidePoly = false;

    Model* model = nullptr;

    // ÉÅÉìÉoïœêîÇ…í«â¡
    std::vector<SpringCapsule>			springCapsules;
};