// Enemy.h

#pragma once

#include "Entity.h"

class Enemy : public Entity
{
public:
	Enemy();
	~Enemy() = default;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void OnCollisionEnter(Actor* other) override;
	void OnCollisionStay(Actor* other) override;
	void OnCollisionExit(Actor* other) override;
	void OnTriggerEnter(Actor* other) override;
	void OnTriggerStay(Actor* other) override;
	void OnTriggerExit(Actor* other) override;

private:

};
