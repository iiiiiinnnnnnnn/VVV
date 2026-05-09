// Player.h

#pragma once

#include "Components.h"
#include "Actor.h"
#include "Weapon.h"
#include "Model.h"
#include "PlayerController.h"

class Player : public Actor
{
public:
	Player();
	~Player() = default;
	void OnUpdate(float elapsedTime) override;
	void OnRender(const RenderContext& rc, float elapsedTime) override;

	void SetController(std::unique_ptr<PlayerController> ctrl) { controller = std::move(ctrl); }
	PlayerController* GetController() const { return controller.get(); }

	Model::Node* GetHandNode() const { return handNode; }

	void SetWeapon(Weapon* newWeapon) { weapon = newWeapon; }
	Weapon* GetWeapon() const { return weapon; }

	Model* GetModel() const { return model.get(); }

private:
	std::unique_ptr<PlayerController> controller;
	Model::Node* handNode = nullptr;
	std::shared_ptr<Model> model = nullptr;
	Weapon* weapon = nullptr;

	float speed;
};
