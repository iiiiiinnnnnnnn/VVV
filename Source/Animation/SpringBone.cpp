// SpringBone.cpp

#include "Animation/SpringBone.h"
#include "Gameplay/Actor/Actor.h"
#include "Rendering/Core/Graphics.h"
#include "Application/Time/GameTime.h"
#include "IconsFontAwesome5.h"

namespace
{
    Vector3 SafeNormalize(const Vector3& value, const Vector3& fallback)
    {
        Vector3 result = value;
        if (result.LengthSquared() <= 0.000001f)
            return fallback;

        result.Normalize();
        return result;
    }

    Quaternion MakeRotationBetweenDirections(
        const Vector3& fromDirection,
        const Vector3& toDirection)
    {
        Vector3 from = SafeNormalize(fromDirection, { 0.0f, 0.0f, 1.0f });
        Vector3 to = SafeNormalize(toDirection, from);

        float dot = from.Dot(to);
        dot = std::clamp(dot, -1.0f, 1.0f);

        if (dot > 0.9999f)
        {
            return Quaternion::Identity;
        }

        if (dot < -0.9999f)
        {
            Vector3 axis = Vector3(1.0f, 0.0f, 0.0f).Cross(from);

            if (axis.LengthSquared() <= 0.000001f)
            {
                axis = Vector3(0.0f, 1.0f, 0.0f).Cross(from);
            }

            axis = SafeNormalize(axis, { 0.0f, 1.0f, 0.0f });

            Quaternion result = Quaternion::CreateFromAxisAngle(axis, DirectX::XM_PI);
            result.Normalize();
            return result;
        }

        Vector3 axis = from.Cross(to);
        axis = SafeNormalize(axis, { 0.0f, 1.0f, 0.0f });

        float angle = acosf(dot);

        Quaternion result = Quaternion::CreateFromAxisAngle(axis, angle);
        result.Normalize();
        return result;
    }

    Matrix MakeBoneDebugTransform(const Vector3& start, const Vector3& end, float& length)
    {
        Vector3 dir = end - start;
        length = dir.Length();

        if (length <= 0.0001f)
            return Matrix::Identity;

        dir /= length;

        Vector3 up = { 0.0f, 1.0f, 0.0f };
        if (fabsf(dir.Dot(up)) > 0.99f)
            up = { 1.0f, 0.0f, 0.0f };

        Vector3 right = up.Cross(dir);
        right.Normalize();

        up = dir.Cross(right);
        up.Normalize();

        DirectX::XMMATRIX world;
        world.r[0] = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&right), length);
        world.r[1] = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&up), length);
        world.r[2] = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&dir), length);
        world.r[3] = DirectX::XMVectorSet(start.x, start.y, start.z, 1.0f);

        Matrix result;
        DirectX::XMStoreFloat4x4(&result, world);
        return result;
    }

    Matrix MakeCapsuleDebugTransform(const Vector3& start, const Vector3& end, float& height)
    {
        Vector3 dir = end - start;
        height = dir.Length();

        if (height <= 0.0001f)
            return Matrix::Identity;

        Vector3 axisY = dir / height;
        Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
        Vector3 axisX;

        if (fabsf(axisY.Dot(worldUp)) > 0.99f)
            axisX = Vector3(1.0f, 0.0f, 0.0f).Cross(axisY);
        else
            axisX = worldUp.Cross(axisY);

        axisX = SafeNormalize(axisX, { 1.0f, 0.0f, 0.0f });

        Vector3 axisZ = axisX.Cross(axisY);
        axisZ = SafeNormalize(axisZ, { 0.0f, 0.0f, 1.0f });

        Vector3 center = (start + end) * 0.5f;

        DirectX::XMMATRIX world;
        world.r[0] = DirectX::XMLoadFloat3(&axisX);
        world.r[1] = DirectX::XMLoadFloat3(&axisY);
        world.r[2] = DirectX::XMLoadFloat3(&axisZ);
        world.r[3] = DirectX::XMVectorSet(center.x, center.y, center.z, 1.0f);

        Matrix result;
        DirectX::XMStoreFloat4x4(&result, world);
        return result;
    }
}

SpringBone::SpringBone(
    Object* owner,
	LayerId layerId,
    VMDLModel* model,
    std::vector<std::string> boneContainNames,
    std::vector<SpringCapsule> bodyCapsules)
    : PhysicsComponent(owner, layerId)
    , springCapsules(bodyCapsules)
    , model(model)
{
    // エラー用
    dynamic_cast<Actor*>(owner);

    BuildBones(boneContainNames);
}

SpringBone::SpringBone(
	Object* owner,
	LayerId layerId,
	VMDLModel* model,
	int rootNodeIndex,
	std::vector<SpringCapsule> bodyCapsules,
	float stiffness,
	float drag)
	: PhysicsComponent(owner, layerId)
	, springCapsules(std::move(bodyCapsules))
	, stiffness(stiffness)
	, damping(drag)
	, model(model)
{
	BuildBones(rootNodeIndex);
}

void SpringBone::BuildBones(int rootNodeIndex)
{
	bones.clear();
	if (!model || rootNodeIndex < 0 || rootNodeIndex >= static_cast<int>(model->GetNodes().size())) return;

	const auto addBone = [this](int nodeIndex)
	{
		const auto& node = model->GetNodes()[nodeIndex];
		Bone bone;
		bone.nodeIndex = nodeIndex;
		bone.localPosition = node.position;
		bone.localRotation = node.rotation;
		bone.worldTransform = node.worldTransform;
		bone.currentWorldPosition = bone.worldTransform.Translation();
		bone.oldWorldPosition = bone.currentWorldPosition;
		bones.push_back(bone);
	};

	std::vector<int> pending{rootNodeIndex};
	while (!pending.empty())
	{
		const int nodeIndex = pending.back();
		pending.pop_back();
		addBone(nodeIndex);
		for (const auto* child : model->GetNodes()[nodeIndex].children)
		{
			pending.push_back(static_cast<int>(child - model->GetNodes().data()));
		}
	}
	std::sort(bones.begin(), bones.end(), [this](const Bone& a, const Bone& b)
	{
		return GetNodeDepth(a.nodeIndex) < GetNodeDepth(b.nodeIndex);
	});
	initialized = false;
}

void SpringBone::BuildBones(const std::vector<std::string>& boneContainNames)
{
    bones.clear();

    if (model == nullptr)
        return;

    std::vector<VMDLModel::Node>& nodes = model->GetNodes();

    for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex)
    {
        VMDLModel::Node& node = nodes[nodeIndex];

        if (!ContainsAnyName(node.name, boneContainNames))
            continue;

        Bone bone;
        bone.nodeIndex = nodeIndex;
        bone.localPosition = node.position;
        bone.localRotation = node.rotation;
        bone.worldTransform = node.worldTransform;
        bone.currentWorldPosition = bone.worldTransform.Translation();
        bone.oldWorldPosition = bone.currentWorldPosition;
        bones.push_back(bone);
    }

    std::sort(bones.begin(), bones.end(),
              [this](const Bone& a, const Bone& b)
    {
        return GetNodeDepth(a.nodeIndex) < GetNodeDepth(b.nodeIndex);
    });

    initialized = false;
}

bool SpringBone::ContainsAnyName(
    const std::string& nodeName,
    const std::vector<std::string>& boneContainNames)
{
    for (const std::string& containName : boneContainNames)
    {
        if (containName.empty())
            continue;

        if (nodeName.find(containName) != std::string::npos)
            return true;
    }

    return false;
}

int SpringBone::GetNodeDepth(int nodeIndex) const
{
    if (model == nullptr)
        return 0;

    const std::vector<VMDLModel::Node>& nodes = model->GetNodes();
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size()))
        return 0;

    int depth = 0;
    const VMDLModel::Node* node = &nodes[nodeIndex];

    while (node != nullptr && node->parent != nullptr)
    {
        ++depth;
        node = node->parent;
    }

    return depth;
}

SpringBone::Bone* SpringBone::FindBone(int nodeIndex)
{
    for (Bone& bone : bones)
    {
        if (bone.nodeIndex == nodeIndex)
            return &bone;
    }

    return nullptr;
}

const SpringBone::Bone* SpringBone::FindBone(int nodeIndex) const
{
    for (const Bone& bone : bones)
    {
        if (bone.nodeIndex == nodeIndex)
            return &bone;
    }

    return nullptr;
}

Matrix SpringBone::GetNodeWorldTransform(int nodeIndex) const
{
    if (nodeIndex < 0 || model == nullptr)
        return Matrix::Identity;

    const Bone* bone = FindBone(nodeIndex);
    if (bone != nullptr)
        return bone->worldTransform;

    const std::vector<VMDLModel::Node>& nodes = model->GetNodes();
    if (nodeIndex >= static_cast<int>(nodes.size()))
        return Matrix::Identity;

    return nodes[nodeIndex].worldTransform;
}

void SpringBone::ApplyCapsuleCollision(Vector3& worldPosition) const
{
    if (model == nullptr)
        return;

    for (const SpringCapsule& capsule : springCapsules)
    {
        if (capsule.radius <= 0.0f)
            continue;

        Matrix capsuleWorld = GetNodeWorldTransform(capsule.nodeIndex);

        Vector3 capStart = Vector3::Transform(capsule.start, capsuleWorld);
        Vector3 capEnd = Vector3::Transform(capsule.end, capsuleWorld);

        Vector3 segment = capEnd - capStart;
        float segmentLengthSq = segment.LengthSquared();

        float t = 0.0f;
        if (segmentLengthSq > 0.00001f)
        {
            t = (worldPosition - capStart).Dot(segment) / segmentLengthSq;
            t = std::clamp(t, 0.0f, 1.0f);
        }

        Vector3 closest = capStart + segment * t;
        Vector3 diff = worldPosition - closest;
        float distance = diff.Length();

        const float effectiveRadius = capsule.radius + collisionRadius;

        if (distance < effectiveRadius)
        {
            if (distance < 0.0001f)
                diff = { 0.0f, 1.0f, 0.0f };
            else
                diff /= distance;

            worldPosition = closest + diff * effectiveRadius;
        }
    }
}

void SpringBone::Reset()
{
    initialized = true;

    if (model == nullptr)
        return;

	Matrix ownerWorldTransform = model->GetWorldTransform();
    model->UpdateTransform(ownerWorldTransform);

    std::vector<VMDLModel::Node>& nodes = model->GetNodes();

    for (Bone& bone : bones)
    {
        if (bone.nodeIndex < 0 || bone.nodeIndex >= static_cast<int>(nodes.size()))
            continue;

        VMDLModel::Node& node = nodes[bone.nodeIndex];
        bone.localPosition = node.position;
        bone.localRotation = node.rotation;
        bone.worldTransform = node.worldTransform;
        bone.currentWorldPosition = bone.worldTransform.Translation();
        bone.oldWorldPosition = bone.currentWorldPosition;
    }
}

void SpringBone::LateUpdate()
{
    if (model == nullptr || bones.empty())
        return;

    if (!initialized)
        Reset();

    float elapsedTime = std::max(Game::Time::deltaTime, 0.0f);
    if (elapsedTime <= 0.0f)
        return;

    std::vector<VMDLModel::Node>& nodes = model->GetNodes();
    const int nodeCount = static_cast<int>(nodes.size());

	Matrix ownerWorldTransform = model->GetWorldTransform();

    model->UpdateTransform(ownerWorldTransform);

    std::vector<Vector3> baseLocalPositions(nodeCount);
    std::vector<Quaternion> baseLocalRotations(nodeCount);
    std::vector<Matrix> baseWorldTransforms(nodeCount);

    for (int i = 0; i < nodeCount; ++i)
    {
        baseLocalPositions[i] = nodes[i].position;
        baseLocalRotations[i] = nodes[i].rotation;
        baseWorldTransforms[i] = nodes[i].worldTransform;
    }

    int subStepCount = 1;
    if (maxSubStepTime > 0.0f)
    {
        subStepCount = static_cast<int>(ceilf(elapsedTime / maxSubStepTime));
        subStepCount = std::clamp(subStepCount, 1, maxSubSteps);
    }

    const float stepTime = elapsedTime / static_cast<float>(subStepCount);

    const float referenceDeltaTime = 1.0f / 60.0f;
    const float stepRate = stepTime / referenceDeltaTime;

    const float dampingBase = std::clamp(damping, 0.0f, 1.0f);
    const float stiffnessBase = std::clamp(stiffness, 0.0f, 1.0f);

    const float stepDamping = powf(dampingBase, stepRate);
    const float stepStiffness = 1.0f - powf(1.0f - stiffnessBase, stepRate);

    const float stepMaxVelocity = maxVelocity > 0.0f
        ? maxVelocity * stepRate
        : 0.0f;

    for (int subStep = 0; subStep < subStepCount; ++subStep)
    {
        for (Bone& bone : bones)
        {
            if (bone.nodeIndex < 0 || bone.nodeIndex >= nodeCount)
                continue;

            VMDLModel::Node& node = nodes[bone.nodeIndex];
            if (node.parent == nullptr)
                continue;

            const int parentNodeIndex = static_cast<int>(node.parent - &nodes[0]);
            Bone* parentBone = FindBone(parentNodeIndex);

            Matrix parentWorldTransform = parentBone != nullptr
                ? parentBone->worldTransform
                : baseWorldTransforms[parentNodeIndex];

            Bone* childBone = nullptr;
            VMDLModel::Node* childNode = nullptr;

            for (VMDLModel::Node* candidateChildNode : node.children)
            {
                const int childNodeIndex = static_cast<int>(candidateChildNode - &nodes[0]);
                Bone* foundChildBone = FindBone(childNodeIndex);

                if (foundChildBone != nullptr)
                {
                    childBone = foundChildBone;
                    childNode = candidateChildNode;
                    break;
                }
            }

            if (childBone == nullptr || childNode == nullptr)
                continue;

            if (childBone->nodeIndex < 0 || childBone->nodeIndex >= nodeCount)
                continue;

            const int boneNodeIndex = bone.nodeIndex;
            const int childNodeIndex = childBone->nodeIndex;

            bone.localPosition = baseLocalPositions[boneNodeIndex];
            childBone->localPosition = baseLocalPositions[childNodeIndex];

            Quaternion baseLocalRotation = baseLocalRotations[boneNodeIndex];
            Quaternion childBaseLocalRotation = baseLocalRotations[childNodeIndex];

            Matrix baseLocalTransform =
                Matrix::CreateFromQuaternion(baseLocalRotation) *
                Matrix::CreateTranslation(bone.localPosition);

            bone.worldTransform = baseLocalTransform * parentWorldTransform;

            Vector3 parentWorldPosition = bone.worldTransform.Translation();

            Vector3 animatedChildWorldPosition =
                Vector3::Transform(childBone->localPosition, bone.worldTransform);

            Vector3 restDirection = animatedChildWorldPosition - parentWorldPosition;
            float boneLength = restDirection.Length();

            if (boneLength <= 0.0001f)
                continue;

            Vector3 currentChildWorldPosition = childBone->currentWorldPosition;

            const float resetDistance = boneLength * 3.0f;
            if ((currentChildWorldPosition - animatedChildWorldPosition).LengthSquared() >
                resetDistance * resetDistance)
            {
                currentChildWorldPosition = animatedChildWorldPosition;
                childBone->currentWorldPosition = animatedChildWorldPosition;
                childBone->oldWorldPosition = animatedChildWorldPosition;
            }

            Vector3 velocity = currentChildWorldPosition - childBone->oldWorldPosition;
            childBone->oldWorldPosition = currentChildWorldPosition;

            velocity *= stepDamping;
            velocity += gravity * stepTime;

            if (stepMaxVelocity > 0.0f)
            {
                const float maxVelocitySq = stepMaxVelocity * stepMaxVelocity;
                if (velocity.LengthSquared() > maxVelocitySq)
                {
                    velocity.Normalize();
                    velocity *= stepMaxVelocity;
                }
            }

            Vector3 nextChildWorldPosition = currentChildWorldPosition + velocity;

            nextChildWorldPosition +=
                (animatedChildWorldPosition - nextChildWorldPosition) * stepStiffness;

            const int iterations = std::max(solverIterations, 1);
            for (int iteration = 0; iteration < iterations; ++iteration)
            {
                Vector3 toChild = nextChildWorldPosition - parentWorldPosition;

                if (toChild.LengthSquared() <= 0.000001f)
                {
                    toChild = restDirection;
                }

                toChild = SafeNormalize(toChild, restDirection);
                nextChildWorldPosition = parentWorldPosition + toChild * boneLength;

                ApplyCapsuleCollision(nextChildWorldPosition);
            }

            ApplyCapsuleCollision(nextChildWorldPosition);

            childBone->currentWorldPosition = nextChildWorldPosition;

            Vector3 localDirection = SafeNormalize(
                childBone->localPosition,
                { 0.0f, 0.0f, 1.0f });

            Matrix inverseBaseWorld = bone.worldTransform.Invert();

            Vector3 localTargetDirection =
                Vector3::Transform(nextChildWorldPosition, inverseBaseWorld);

            localTargetDirection = SafeNormalize(localTargetDirection, localDirection);

            Quaternion springRotation = MakeRotationBetweenDirections(
                localDirection,
                localTargetDirection);

            bone.localRotation = springRotation * baseLocalRotation;
            bone.localRotation.Normalize();

            Matrix finalLocalTransform =
                Matrix::CreateFromQuaternion(bone.localRotation) *
                Matrix::CreateTranslation(bone.localPosition);

            bone.worldTransform = finalLocalTransform * parentWorldTransform;

            childBone->localRotation = childBaseLocalRotation;

            Matrix childLocalTransform =
                Matrix::CreateFromQuaternion(childBone->localRotation) *
                Matrix::CreateTranslation(childBone->localPosition);

            childBone->worldTransform = childLocalTransform * bone.worldTransform;
        }
    }

    for (Bone& bone : bones)
    {
        if (bone.nodeIndex < 0 || bone.nodeIndex >= nodeCount)
            continue;

        nodes[bone.nodeIndex].rotation = bone.localRotation;
    }

    model->UpdateTransform(ownerWorldTransform);

    for (Bone& bone : bones)
    {
        if (bone.nodeIndex < 0 || bone.nodeIndex >= nodeCount)
            continue;

        bone.worldTransform = nodes[bone.nodeIndex].worldTransform;
    }
}

void SpringBone::Render(const RenderContext& rc)
{
    if (!showDebug) return;

    Game::Graphics& graphics = Game::Graphics::Instance();
    PrimitiveRenderer* primitiveRenderer = graphics.GetPrimitiveRenderer();
    ShapeRenderer* shapeRenderer = graphics.GetShapeRenderer();

    if (primitiveRenderer == nullptr || shapeRenderer == nullptr)
        return;

    const std::vector<VMDLModel::Node>& nodes = model->GetNodes();

    if (drawBones)
    {
        for (const Bone& bone : bones)
        {
            if (bone.nodeIndex < 0 || bone.nodeIndex >= static_cast<int>(nodes.size()))
                continue;

            const VMDLModel::Node& node = nodes[bone.nodeIndex];

            const Bone* childBone = nullptr;
            for (VMDLModel::Node* childNode : node.children)
            {
                const int childNodeIndex = static_cast<int>(childNode - &nodes[0]);
                childBone = FindBone(childNodeIndex);

                if (childBone != nullptr)
                    break;
            }

            if (childBone == nullptr)
                continue;

            Vector3 parentPosition = bone.worldTransform.Translation();
            Vector3 childPosition = childBone->worldTransform.Translation();

            float length = 0.0f;
            Matrix debugTransform = MakeBoneDebugTransform(parentPosition, childPosition, length);

            if (length <= 0.0001f)
                continue;

            primitiveRenderer->DrawAxis(debugTransform, { 1.0f, 1.0f, 0.0f, 1.0f });
            shapeRenderer->DrawBone(debugTransform, length, { 1.0f, 1.0f, 0.0f, 1.0f });
        }
    }

    if (drawCapsules)
    {
        for (const SpringCapsule& capsule : springCapsules)
        {
            if (capsule.radius <= 0.0f)
                continue;

            Matrix capsuleWorld = GetNodeWorldTransform(capsule.nodeIndex);
            Vector3 capStart = Vector3::Transform(capsule.start, capsuleWorld);
            Vector3 capEnd = Vector3::Transform(capsule.end, capsuleWorld);

            float height = 0.0f;
            Matrix capsuleTransform = MakeCapsuleDebugTransform(capStart, capEnd, height);

            if (height <= 0.0001f)
                continue;

            shapeRenderer->DrawCapsule(
                capsuleTransform,
                capsule.radius,
                height,
                { 0.0f, 1.0f, 1.0f, 0.5f });
        }
    }
}

void SpringBone::DrawGUI()
{
    ImGui::Text("Bones: %d", static_cast<int>(bones.size()));
    ImGui::Text("Capsules: %d", static_cast<int>(springCapsules.size()));

    if (ImGui::Button("Reset Physics"))
        Reset();

    ImGui::Separator();
    ImGui::DragFloat3("Gravity", &gravity.x, 0.01f, -10.0f, 10.0f, "%.3f");
    ImGui::DragFloat("Damping", &damping, 0.001f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("Max Velocity", &maxVelocity, 0.01f, 0.0f, 10.0f, "%.3f");

    ImGui::Separator();
    ImGui::Text("Collision / Solver");
    ImGui::DragFloat("Collision Radius", &collisionRadius, 0.001f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("Stiffness", &stiffness, 0.001f, 0.0f, 1.0f, "%.3f");
    ImGui::DragFloat("Max SubStep Time", &maxSubStepTime, 0.0001f, 0.001f, 0.1f, "%.4f");
    ImGui::DragInt("Max SubSteps", &maxSubSteps, 1, 1, 16);
    ImGui::DragInt("Solver Iterations", &solverIterations, 1, 1, 12);

    ImGui::Checkbox("Draw Bones", &drawBones);
    ImGui::Checkbox("Draw Capsules", &drawCapsules);

    ImGui::Separator();
    ImGui::Text("Capsule Colliders");

    if (ImGui::Button("Add Capsule"))
    {
        SpringCapsule capsule;
        springCapsules.push_back(capsule);
    }

    if (model != nullptr)
    {
        std::vector<VMDLModel::Node>& nodes = model->GetNodes();
        int deleteIndex = -1;

        for (int i = 0; i < static_cast<int>(springCapsules.size()); ++i)
        {
            SpringCapsule& capsule = springCapsules[i];

            ImGui::PushID(i);

            const std::string header = "Capsule[" + std::to_string(i) + "]";
            if (ImGui::CollapsingHeader(header.c_str()))
            {
                std::string currentNodeName = "None";
                if (capsule.nodeIndex >= 0 && capsule.nodeIndex < static_cast<int>(nodes.size()))
                    currentNodeName = nodes[capsule.nodeIndex].name;

                if (ImGui::BeginCombo("Node", currentNodeName.c_str()))
                {
                    if (ImGui::Selectable("None", capsule.nodeIndex < 0))
                        capsule.nodeIndex = -1;

                    for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex)
                    {
                        const bool selected = capsule.nodeIndex == nodeIndex;
                        if (ImGui::Selectable(nodes[nodeIndex].name.c_str(), selected))
                            capsule.nodeIndex = nodeIndex;

                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }

                ImGui::DragFloat3("Start", &capsule.start.x, 0.01f);
                ImGui::DragFloat3("End", &capsule.end.x, 0.01f);
                ImGui::DragFloat("Radius", &capsule.radius, 0.005f, 0.001f, 10.0f, "%.3f");

                if (ImGui::Button("Delete"))
                    deleteIndex = i;
            }

            ImGui::PopID();
        }

        if (deleteIndex >= 0)
            springCapsules.erase(springCapsules.begin() + deleteIndex);
    }
}
