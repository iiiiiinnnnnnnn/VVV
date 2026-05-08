// Player.h

#pragma once

#include "Common.h"
#include "Components.h"
#include "Actor.h"
#include "Animator.h"
#include "Weapon.h"
#include "Model.h"

class Player : public Actor
{
public:
	Player();
	~Player() = default;
	void OnUpdate(float elapsedTime) override;
	void OnRender(const RenderContext& rc, float elapsedTime) override;

	Model::Node* GetHandNode() const { return handNode; }

	void SetWeapon(Weapon* newWeapon) { weapon = newWeapon; }

private:
	Model::Node* handNode = nullptr;
	Weapon* weapon = nullptr;
};
