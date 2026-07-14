// BoneFollower.h

#pragma once
#include <string>

#include "Core/Object/Component.h"
#include "Resource/Model.h"
#include "Core/Object/Transform.h"

class Rigidbody;
class Actor;

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

	void OnAwake() override;
	void OnUpdate() override;
	void OnDrawGUI() override;
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
