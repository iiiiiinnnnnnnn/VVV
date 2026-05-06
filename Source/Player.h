// Player.h

#pragma once

#include "Common.h"
#include "RenderContext.h"
#include "Model.h"
#include "Animator.h"
#include "ModelRenderer.h"

class Player
{
public:
	Player();
	~Player() = default;
	void Update(float elapsedTime);
	void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer);

	struct Transform
	{
		Vector3 position = Vector3::Zero;
		Quaternion rotation = Quaternion::Identity;
		Vector3 scale = Vector3::One;
		Vector3 forward = Vector3::Zero;
		Matrix matrix = Matrix::Identity;
		void Update()
		{
			matrix = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
			forward = Vector3::TransformNormal(Vector3::UnitZ, matrix);
		}
	} transform;

	Model* GetModel() const { return model.get(); }
	Animator* GetAnimator() const { return animator.get(); }

private:
	std::shared_ptr<Model> model;
	std::shared_ptr<Animator> animator;
};
