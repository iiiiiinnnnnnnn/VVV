// Actor.h

#pragma once

#include "RenderContext.h"

class Actor {
public:
	virtual ~Actor() = default;
	virtual void Update(float elapsedTime) = 0;

protected:
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

public:
	Transform GetTransform() const { return transform; }
};
