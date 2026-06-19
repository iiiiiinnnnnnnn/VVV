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
	if (ImGui::TreeNode(ICON_FA_BULLSEYE " DamageHoleComponent"))
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

				AddDamageHoleAt(center + direction * ComputeSurfaceDistance(*actor));
			}
		}

		if (ImGui::Button("Clear Holes"))
		{
			ClearDamageHoles();
		}

		ImGui::TreePop();
	}
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

	AddDamageHoleAt(center + direction * ComputeSurfaceDistance(*actor));
}

void DamageHoleComponent::AddDamageHoleFromPosition(const Vector3& hitPosition)
{
	Actor* actor = GetOwnerAsActor();
	if (actor == nullptr)
	{
		return;
	}

	Vector3 center = actor->transform.position;
	if (Rigidbody* rb = actor->GetComponent<Rigidbody>())
	{
		center = rb->GetPosition();
	}

	Vector3 direction = hitPosition - center;
	if (direction.LengthSquared() < eps)
	{
		direction = actor->transform.forward;
	}

	direction.Normalize();
	AddDamageHoleAt(center + direction * ComputeSurfaceDistance(*actor));
}

void DamageHoleComponent::AddDamageHoleAt(const Vector3& position)
{
	if (damageHoles.size() >= MaxDamageHoles)
	{
		damageHoles.erase(damageHoles.begin());
	}

	damageHoles.emplace_back(position.x, position.y, position.z, holeRadius);
	UpdateShaderParams();
}

void DamageHoleComponent::ClearDamageHoles()
{
	damageHoles.clear();
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

	for (int i = 0; i < MaxDamageHoles; ++i)
	{
		const Vector4 hole = (i < static_cast<int>(damageHoles.size()))
			? damageHoles[i]
			: Vector4(0, 0, 0, 0);

		modelRenderer->SetShaderParamForAllMaterials({"hole" + std::to_string(i), hole});
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
