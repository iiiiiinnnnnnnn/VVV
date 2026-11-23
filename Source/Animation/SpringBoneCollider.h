#pragma once
#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"
#include "Resource/VMDLModel.h"

class ShapeRenderer;

// PhysXには登録せず、スプリングの点とカプセルの衝突だけを扱う
class SpringBoneCollider : public Component
{
public:
	SpringBoneCollider(Object* owner, VMDLModel* model, const VMDLModel::VmdlSpringCollider& settings);
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BULLSEYE " 揺れもの用コライダー"; }
	void ResolveCollision(Vector3& position, float pointRadius) const;
	Vector3 start = Vector3::Zero;
	Vector3 end = {0.0f, 0.1f, 0.0f};
	float radius = 0.1f;
	int nodeIndex = -1;

	void ResolveCollision(Vector3& position, const Matrix& world, float pointRadius) const;
	void Render(ShapeRenderer& renderer, const Matrix& world) const;
	bool DrawGUI(const VMDLModel& model);

private:
	Matrix GetWorldTransform() const;
	VMDLModel* model;
};
