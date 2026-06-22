// BoneFollower.cpp

#include "BoneFollower.h"

#include "Actor.h"
#include "Rigidbody.h"
#include "imgui.h"
#include "IconsFontAwesome5.h"

BoneFollower::BoneFollower(
	Object* owner,
	Model* targetModel,
	const std::string& targetNodeName,
	const Transform& offset)
	: Component(owner)
	, offset(offset)
{
	Actor* ownerActor = Component::GetOwnerAsActor();
	this->offset.Update();
	SetTarget(targetModel, targetNodeName);
	ApplyFollow();
}

BoneFollower::BoneFollower(
	Object* owner,
	Model* targetModel,
	int targetNodeIndex,
	const Transform& offset)
	: Component(owner)
	, offset(offset)
{
	Actor* ownerActor = Component::GetOwnerAsActor();
	this->offset.Update();
	SetTarget(targetModel, targetNodeIndex);
	ApplyFollow();
}

void BoneFollower::Update()
{
	ApplyFollow();
}

void BoneFollower::DrawGUI()
{
	if (ImGui::TreeNode(ICON_FA_BONE " BoneFollower"))
	{
		ImGui::Text("Target Node: [%d]%s", targetNodeIndex, targetNodeName.c_str());
		offset.DrawGUI();
		ImGui::TreePop();
	}
}

void BoneFollower::SetTarget(Model* targetModel, const std::string& targetNodeName)
{
	this->targetModel = targetModel;
	this->targetNodeName = targetNodeName;
	targetNodeIndex = targetModel ? targetModel->GetNodeIndex(targetNodeName.c_str()) : -1;
}

void BoneFollower::SetTarget(Model* targetModel, int targetNodeIndex)
{
	this->targetModel = targetModel;
	this->targetNodeIndex = targetNodeIndex;
	targetNodeName.clear();

	if (!targetModel) return;

	const std::vector<Model::Node>& nodes = targetModel->GetNodes();
	if (targetNodeIndex < 0 || targetNodeIndex >= static_cast<int>(nodes.size())) return;

	targetNodeName = nodes[targetNodeIndex].name;
}

void BoneFollower::SetOffset(const Transform& offset)
{
	this->offset = offset;
	this->offset.Update();
}

void BoneFollower::SetOffset(
	const Vector3& position,
	const Quaternion& rotation,
	const Vector3& scale)
{
	offset.position = position;
	offset.rotation = rotation;
	offset.scale = scale;
	offset.Update();
}

void BoneFollower::ApplyFollow()
{
	Actor* ownerActor = Component::GetOwnerAsActor();
	if (!ownerActor || !targetModel) return;

	const std::vector<Model::Node>& nodes = targetModel->GetNodes();
	if (targetNodeIndex < 0 || targetNodeIndex >= static_cast<int>(nodes.size())) return;

	Vector3 targetScale;
	Quaternion targetRotation;
	Vector3 targetPosition;
	Matrix targetWorldTransform = nodes[targetNodeIndex].worldTransform;
	targetWorldTransform.Decompose(targetScale, targetRotation, targetPosition);

	Matrix targetTransform =
		Matrix::CreateScale(targetScale) *
		Matrix::CreateFromQuaternion(targetRotation) *
		Matrix::CreateTranslation(targetPosition);
	Matrix worldTransform = offset.matrix * targetTransform;

	worldTransform.Decompose(
		ownerActor->transform.scale,
		ownerActor->transform.rotation,
		ownerActor->transform.position);

	ownerActor->transform.Update();

	SyncRigidbody(ownerActor);
}

void BoneFollower::SyncRigidbody(Actor* ownerActor)
{
	Rigidbody* rb = ownerActor->GetComponent<Rigidbody>();
	if (!rb) return;

	rb->SetPosition(ownerActor->transform.position);
	rb->SetRotation(ownerActor->transform.rotation);

	RigidbodyDynamic* dynamicRb = dynamic_cast<RigidbodyDynamic*>(rb);
	if (dynamicRb)
		dynamicRb->SetVelocity(Vector3::Zero);
}
