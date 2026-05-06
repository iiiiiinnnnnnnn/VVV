// RenderActor.h

#pragma once

#include "Actor.h"
#include "Model.h"
#include "ModelRenderer.h"

#include "Graphics.h"

class RenderActor : virtual public Actor
{
public:
	RenderActor(const char* filename)
	{
		auto device = Graphics::Instance().GetDevice();
		model = std::make_shared<Model>(device, filename);
	}
	virtual void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer) = 0;

	Model* GetModel() const { return model.get(); }

protected:
	std::shared_ptr<Model> model;
};
