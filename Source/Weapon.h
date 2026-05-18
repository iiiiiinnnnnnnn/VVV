// Weapon.h

#pragma once

#include "Common.h"
#include "Components.h"
#include "Actor.h"

class Character;

class Weapon : public Actor
{
public:
	Weapon(Character* character);
	~Weapon() = default;
	void OnUpdate(float elapsedTime) override;
	void OnLateUpdate(float elapsedTime) override;
	void OnRender(const RenderContext& rc, float elapsedTime) override;
	void OnDrawGUI(float elapsedTime) override;

private:

	Character* character = nullptr;
};
