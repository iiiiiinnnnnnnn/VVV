// Player.h

#pragma once

#include "Components.h"
#include "Actor.h"
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

	Model* GetModel() const { return model.get(); }

private:
	Model::Node* handNode = nullptr;
	std::shared_ptr<Model> model = nullptr;
	Weapon* weapon = nullptr;
};
