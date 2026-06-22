// DamageHoleComponent.h

#pragma once

#include "Component.h"
#include "Common.h"

class Actor;
class ModelRenderComponent;

class DamageHoleComponent : public Component
{
public:
	DamageHoleComponent(
		Object* owner,
		ModelRenderComponent* modelRenderer,
		float holeRadius,
		float holeEdgeWidth,
		float holeDepth,
		float surfaceDistance = -1.0f);

	void DrawGUI() override;
	void LateUpdate() override;

	void AddDamageHoleFrom(const Actor* attacker);
	void AddDamageHoleFromPosition(const Vector3& hitPosition);
	void AddDamageHoleFromPosition(const Vector3& hitPosition, const Vector3& dentDirection);
	void AddDamageHoleAt(const Vector3& position, const Vector3& dentDirection = Vector3::Zero);
	void ClearDamageHoles();

	void SetHoleRadius(float radius);
	void SetHoleEdgeWidth(float edgeWidth);
	void SetHoleDepth(float depth);
	void SetSurfaceDistance(float distance) { surfaceDistance = distance; }

private:
	static constexpr int MaxDamageHoles = 8;

	void UpdateShaderParams();
	float ComputeSurfaceDistance(const Actor& actor) const;
	Vector4 MakeWorldHole(const Vector4& localHole, const Actor* actor) const;
	Vector4 MakeWorldDirection(const Vector4& localDirection, const Actor* actor) const;

	ModelRenderComponent* modelRenderer = nullptr;
	std::vector<Vector4> damageHoles;
	std::vector<Vector4> damageHoleDirections;
	float holeRadius = 1.0f;
	float holeEdgeWidth = 0.1f;
	float holeDepth = 0.4f;
	float surfaceDistance = -1.0f;
};
