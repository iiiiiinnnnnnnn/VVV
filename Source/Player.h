// Player.h

#pragma once

#include "Common.h"
#include "DynamicActor.h"
#include "AnimatedRenderActor.h"
#include "Animator.h"

class Player : public DynamicActor, public AnimatedRenderActor
{
public:
	Player();
	~Player() = default;
	void Update(float elapsedTime) override;
	void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer) override;

private:

};
