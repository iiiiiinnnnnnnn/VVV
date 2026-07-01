// BoneFollower.h

#pragma once

#include "Component.h"
#include "Model.h"
#include "Transform.h"

class Rigidbody;

class BoneFollower : public Component
{
public:
	BoneFollower(
		Object* owner,
		Model* targetModel,
		const std::string& targetNodeName,
		const Transform& offset = {});

	BoneFollower(
		Object* owner,
		Model* targetModel,
		int targetNodeIndex,
		const Transform& offset = {});

	void Update() override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BONE " BoneFollower"; }

	void SetTarget(Model* targetModel, const std::string& targetNodeName);
	void SetTarget(Model* targetModel, int targetNodeIndex);
	void SetOffset(const Transform& offset);
	void SetOffset(
		const Vector3& position,
		const Quaternion& rotation = Quaternion::Identity,
		const Vector3& scale = Vector3::One);

private:
	void ApplyFollow();
	void SyncRigidbody(Actor* ownerActor);

	Model* targetModel = nullptr;
	int targetNodeIndex = -1;
	std::string targetNodeName;
	Transform offset;
};
