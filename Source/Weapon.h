// Weapon.h

#pragma once

#include "Common.h"
#include "Components.h"
#include "Actor.h"
#include "Animator.h"

class Player;

class Weapon : public Actor
{
public:
	Weapon(Player* player);
	~Weapon() = default;
	void OnUpdate(float elapsedTime) override;
	void OnRender(const RenderContext& rc, float elapsedTime) override;
	void OnDrawGUI(float elapsedTime) override;

private:

	Player* player = nullptr;
};
