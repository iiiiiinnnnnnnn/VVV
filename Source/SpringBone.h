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

        Vector3 localPosition = Vector3::Zero;
        Quaternion localRotation = Quaternion::Identity;

        Matrix worldTransform = Matrix::Identity;
        Vector3 currentWorldPosition = Vector3::Zero;
        Vector3 oldWorldPosition = Vector3::Zero;
    };

    struct SpringCapsule
    {
        Vector3 start = Vector3::Zero;
        Vector3 end = { 0.0f, 0.1f, 0.0f };
        float   radius = 0.1f;
        int     nodeIndex = -1;
    };

    SpringBone(
        Object* owner,
        Model* model,
        std::vector<std::string> boneContainNames,
        std::vector<SpringCapsule> bodyCapsules = {});

    ~SpringBone() override = default;

    void LateUpdate() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;

    void Reset();

private:
    void BuildBones(const std::vector<std::string>& boneContainNames);

    Bone* FindBone(int nodeIndex);
    const Bone* FindBone(int nodeIndex) const;

    Matrix GetNodeWorldTransform(int nodeIndex) const;
    void ApplyCapsuleCollision(Vector3& worldPosition) const;
    int GetNodeDepth(int nodeIndex) const;

    static bool ContainsAnyName(
        const std::string& nodeName,
        const std::vector<std::string>& boneContainNames);

private:
    std::vector<Bone> bones;
    std::vector<SpringCapsule> springCapsules;

    Vector3 gravity = { 0.0f, -0.3f, 0.0f };
    float damping = 0.5f;
    float maxVelocity = 0.3f;

    float collisionRadius = 0.03f;
    float stiffness = 0.05f;
    float maxSubStepTime = 1.0f / 120.0f;
    int maxSubSteps = 16;
    int solverIterations = 4;

    bool drawBones = true;
    bool drawCapsules = true;
    bool initialized = false;

    Model* model = nullptr;
};