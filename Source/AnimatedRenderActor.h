// AnimatedRenderActor.h

#pragma once

#include "RenderActor.h"
#include "Animator.h"

class AnimatedRenderActor : public RenderActor
{
public:
	AnimatedRenderActor(const char* filename) : RenderActor(filename) {
		animator = std::make_shared<Animator>(model.get());
	}
	Animator* GetAnimator() const { return animator.get(); }

protected:
	std::shared_ptr<Animator> animator;
};
