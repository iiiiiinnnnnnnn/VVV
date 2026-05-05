// Stage.h

#pragma once

#include "Common.h"
#include "RenderContext.h"
#include "Model.h"
#include "ModelRenderer.h"

class Stage
{
public:
	Stage();
	~Stage() = default;
	void Update(float elapsedTime);
	void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer);

	struct Transform
	{
		Vector3 position;
		Quaternion rotation;
		Vector3 scale;
		Vector3 forward;
	} transform;

	Model* GetModel() const { return model.get(); }

private:
	std::shared_ptr<Model> model;
};
