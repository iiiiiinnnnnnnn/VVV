#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "Physics/Core/PhysicsComponent.h"
#include "Resource/VMDLModel.h"

class SpringBone : public PhysicsComponent
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
		LayerId layerId,
        VMDLModel* model,
        std::vector<std::string> boneContainNames,
        std::vector<SpringCapsule> bodyCapsules = {});
	SpringBone(
		Object* owner,
		LayerId layerId,
		VMDLModel* model,
		int rootNodeIndex,
		std::vector<SpringCapsule> bodyCapsules,
		float stiffness,
		float drag);

    ~SpringBone() override = default;

    void LateUpdate() override;
    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BONE " SpringBone"; }

    void Reset();
	void SetUseUnscaledTime(bool value) { useUnscaledTime = value; }
    void SetMaxAngle(float degrees) { maxAngleDegrees = std::clamp(degrees, 0.0f, 180.0f); }
    void SetAngleLimits(const Vector3& minimum, const Vector3& maximum);

private:
    void BuildBones(const std::vector<std::string>& boneContainNames);
	void BuildBones(int rootNodeIndex);

    Bone* FindBone(int nodeIndex);
    const Bone* FindBone(int nodeIndex) const;

    Matrix GetNodeWorldTransform(int nodeIndex) const;
    void ApplyCapsuleCollision(Vector3& worldPosition) const;
    int GetNodeDepth(int nodeIndex) const;
    void ApplyAngleLimit(Vector3& position, const Vector3& origin,
        const Vector3& restDirection, float boneLength) const;
    Quaternion ApplyAxisAngleLimits(Vector3& position, const Matrix& baseWorld,
        const Vector3& localDirection, float boneLength) const;

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
    float maxAngleDegrees = 180.0f;
    Vector3 minAnglesDegrees = { -180.0f, -180.0f, -180.0f };
    Vector3 maxAnglesDegrees = { 180.0f, 180.0f, 180.0f };
    float maxSubStepTime = 1.0f / 120.0f;
    int maxSubSteps = 16;
    int solverIterations = 4;

    bool drawBones = true;
    bool drawCapsules = true;
    bool initialized = false;
	bool useUnscaledTime = false;

    VMDLModel* model = nullptr;
};
