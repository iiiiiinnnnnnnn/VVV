// Enemy.h

#pragma once

#include "Components.h"
#include "Actor.h"

class Enemy : public Actor
{
public:
	Enemy();
	~Enemy() = default;
	void OnUpdate() override;
	void OnDrawGUI() override;

private:

};
