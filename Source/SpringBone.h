// SpringBone.h
#pragma once

#include "Component.h"
#include "Model.h"

class SpringBone : public Component
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

    SpringBone(
        Object* owner,
        Model* model,
        std::vector<std::string> boneContainNames,
        std::vector<SpringCapsule> bodycapsules = {});

    ~SpringBone() override = default;

    void LateUpdate() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

private:
    std::vector<Bone>					bones;
    std::vector<SpringCapsule>			springCapsules;

    Model* model = nullptr;
};