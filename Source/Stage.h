// Stage.h

#pragma once

#include "Common.h"
#include "StaticActor.h"
#include "RenderActor.h"

class Stage : public StaticActor, public RenderActor
{
public:
	Stage();
	~Stage() = default;
	void Update(float elapsedTime) override;
	void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer) override;

private:

};
