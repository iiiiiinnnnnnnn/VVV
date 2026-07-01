// DamageHoleComponent.cpp

#include "DamageHoleComponent.h"

#include "Actor.h"
#include "ModelRenderComponent.h"
#include "Rigidbody.h"
#include "imgui.h"
#include "IconsFontAwesome5.h"

DamageHoleComponent::DamageHoleComponent(
	Object* owner,
	ModelRenderComponent* modelRenderer,
	float holeRadius,
	float holeEdgeWidth,
	float holeDepth,
	float surfaceDistance)
	: Component(owner)
	, modelRenderer(modelRenderer)
	, holeRadius(holeRadius)
	, holeEdgeWidth(holeEdgeWidth)
	, holeDepth(holeDepth)
	, surfaceDistance(surfaceDistance)
{
	UpdateShaderParams();
}

void DamageHoleComponent::DrawGUI()
{
	if (ImGui::DragFloat("Hole Radius", &holeRadius, 0.01f, 0.001f, 100.0f))
	{
		UpdateShaderParams();
	}
	if (ImGui::DragFloat("Hole Edge Width", &holeEdgeWidth, 0.01f, 0.001f, 30.0f))
	{
		UpdateShaderParams();
	}
	if (ImGui::DragFloat("Hole Depth", &holeDepth, 0.01f, 0.0f, 100.0f))
	{
		UpdateShaderParams();
	}
	ImGui::DragFloat("Surface Distance", &surfaceDistance, 0.01f, -1.0f, 100.0f);
	ImGui::Text("Hole Count: %d / %d", static_cast<int>(damageHoles.size()), MaxDamageHoles);

	if (ImGui::Button("Add Front Hole"))
	{
		if (Actor* actor = GetOwnerAsActor())
		{
			Vector3 center = actor->transform.position;
			if (Rigidbody* rb = actor->GetComponent<Rigidbody>())
			{
				center = rb->GetPosition();
			}

			Vector3 direction = actor->transform.forward;
			if (direction.LengthSquared() < eps)
			{
				direction = Vector3::Forward;
			}
			direction.Normalize();

			AddDamageHoleAt(center + direction * ComputeSurfaceDistance(*actor), direction);
		}
	}

	if (ImGui::Button("Clear Holes"))
	{
		ClearDamageHoles();
	}
}

void DamageHoleComponent::LateUpdate()
{
	if (damageHoles.empty()) return;

	UpdateShaderParams();
}

void DamageHoleComponent::AddDamageHoleFrom(const Actor* attacker)
{
	if (!attacker) return;

	Actor* actor = GetOwnerAsActor();
	if (!actor) return;

	Vector3 center = actor->transform.position;
	if (Rigidbody* rb = actor->GetComponent<Rigidbody>())
	{
		center = rb->GetPosition();
	}

	Vector3 direction = attacker->transform.position - center;
	if (direction.LengthSquared() < eps)
	{
		direction = Vector3::Forward;
	}
	direction.Normalize();

	AddDamageHoleAt(center + direction * ComputeSurfaceDistance(*actor), direction);
}

void DamageHoleComponent::AddDamageHoleFromPosition(const Vector3& hitPosition)
{
	Actor* actor = GetOwnerAsActor();
	Vector3 direction = actor ? hitPosition - actor->transform.position : Vector3::Zero;
	if (direction.LengthSquared() > eps)
		direction.Normalize();

	AddDamageHoleAt(hitPosition, direction);
}

void DamageHoleComponent::AddDamageHoleFromPosition(const Vector3& hitPosition, const Vector3& dentDirection)
{
	AddDamageHoleAt(hitPosition, dentDirection);
}

void DamageHoleComponent::AddDamageHoleAt(const Vector3& position, const Vector3& dentDirection)
{
	if (damageHoles.size() >= MaxDamageHoles)
	{
		damageHoles.erase(damageHoles.begin());
		if (!damageHoleDirections.empty())
			damageHoleDirections.erase(damageHoleDirections.begin());
	}

	Vector3 direction = dentDirection;
	if (direction.LengthSquared() > eps)
		direction.Normalize();

	Vector3 localPosition = position;
	Vector3 localDirection = direction;
	if (Actor* actor = GetOwnerAsActor())
	{
		Matrix inverseWorld = actor->transform.matrix.Invert();
		localPosition = Vector3::Transform(position, inverseWorld);
		localDirection = Vector3::TransformNormal(direction, inverseWorld);
		if (localDirection.LengthSquared() > eps)
			localDirection.Normalize();
	}

	damageHoles.emplace_back(localPosition.x, localPosition.y, localPosition.z, holeRadius);
	damageHoleDirections.emplace_back(localDirection.x, localDirection.y, localDirection.z, 0.0f);
	UpdateShaderParams();
}

void DamageHoleComponent::ClearDamageHoles()
{
	damageHoles.clear();
	damageHoleDirections.clear();
	UpdateShaderParams();
}

void DamageHoleComponent::SetHoleRadius(float radius)
{
	holeRadius = radius;
	UpdateShaderParams();
}

void DamageHoleComponent::SetHoleEdgeWidth(float edgeWidth)
{
	holeEdgeWidth = edgeWidth;
	UpdateShaderParams();
}

void DamageHoleComponent::SetHoleDepth(float depth)
{
	holeDepth = depth;
	UpdateShaderParams();
}

void DamageHoleComponent::UpdateShaderParams()
{
	if (modelRenderer == nullptr)
	{
		return;
	}

	modelRenderer->SetShaderParamForAllMaterials({"holeCount", static_cast<int>(damageHoles.size())});
	modelRenderer->SetShaderParamForAllMaterials({"holeEdgeWidth", holeEdgeWidth});
	modelRenderer->SetShaderParamForAllMaterials({"holeDepth", holeDepth});

	Actor* actor = GetOwnerAsActor();
	for (int i = 0; i < MaxDamageHoles; ++i)
	{
		const Vector4 hole = (i < static_cast<int>(damageHoles.size()))
			? MakeWorldHole(damageHoles[i], actor)
			: Vector4(0, 0, 0, 0);
		const Vector4 direction = (i < static_cast<int>(damageHoleDirections.size()))
			? MakeWorldDirection(damageHoleDirections[i], actor)
			: Vector4(0, 0, 0, 0);

		modelRenderer->SetShaderParamForAllMaterials({"hole" + std::to_string(i), hole});
		modelRenderer->SetShaderParamForAllMaterials({"holeDirection" + std::to_string(i), direction});
	}
}

float DamageHoleComponent::ComputeSurfaceDistance(const Actor& actor) const
{
	if (surfaceDistance >= 0.0f)
	{
		return surfaceDistance;
	}

	return (std::max)(actor.transform.scale.x, (std::max)(actor.transform.scale.y, actor.transform.scale.z)) * 0.45f;
}

Vector4 DamageHoleComponent::MakeWorldHole(const Vector4& localHole, const Actor* actor) const
{
	if (!actor) return localHole;

	Vector3 localPosition(localHole.x, localHole.y, localHole.z);
	Vector3 worldPosition = Vector3::Transform(localPosition, actor->transform.matrix);
	return Vector4(worldPosition.x, worldPosition.y, worldPosition.z, localHole.w);
}

Vector4 DamageHoleComponent::MakeWorldDirection(const Vector4& localDirection, const Actor* actor) const
{
	Vector3 direction(localDirection.x, localDirection.y, localDirection.z);
	if (actor)
		direction = Vector3::TransformNormal(direction, actor->transform.matrix);

	if (direction.LengthSquared() > eps)
		direction.Normalize();

	return Vector4(direction.x, direction.y, direction.z, 0.0f);
}
