// NavMeshObstacle.cpp

#include "NavMeshObstacle.h"
#include "Actor.h"

bool NavMeshObstacle::GetBounds(Vector3& center, Vector3& size) const
{
	Actor* actor = GetOwnerAsActor();
	if (!actor) return false;

	BoxCollider* box = actor->GetComponent<BoxCollider>();
	if (!box) return false;

	const Vector3 actorScale = actor->transform.scale;
	const Vector3 localPosition = box->GetLocalPosition();
	const Vector3 boxSize = box->GetSize();
	const Matrix rotation = Matrix::CreateFromQuaternion(actor->transform.rotation);

	const Vector3 scaledLocalPosition(
		localPosition.x * actorScale.x,
		localPosition.y * actorScale.y,
		localPosition.z * actorScale.z);

	const Vector3 scaledSize(
		boxSize.x * actorScale.x,
		boxSize.y * actorScale.y,
		boxSize.z * actorScale.z);

	center =
		actor->transform.position +
		Vector3::TransformNormal(scaledLocalPosition, rotation);

	const Vector3 halfSize = scaledSize * 0.5f;
	const Vector3 axisX =
		Vector3::TransformNormal(Vector3::UnitX, rotation) * halfSize.x;
	const Vector3 axisY =
		Vector3::TransformNormal(Vector3::UnitY, rotation) * halfSize.y;
	const Vector3 axisZ =
		Vector3::TransformNormal(Vector3::UnitZ, rotation) * halfSize.z;

	const Vector3 worldHalfSize(
		std::abs(axisX.x) + std::abs(axisY.x) + std::abs(axisZ.x),
		std::abs(axisX.y) + std::abs(axisY.y) + std::abs(axisZ.y),
		std::abs(axisX.z) + std::abs(axisY.z) + std::abs(axisZ.z));

	size = worldHalfSize * 2.0f;
	return true;
}

void NavMeshObstacle::DrawGUI()
{
	if (!ImGui::TreeNode("NavMesh Obstacle")) return;
	ImGui::Text("This actor is marked as a NavMesh obstacle.");
	ImGui::Text("Bounds are determined by the BoxCollider component.");
	ImGui::TreePop();
}
