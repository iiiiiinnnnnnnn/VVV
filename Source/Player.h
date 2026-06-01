// Player.h

#pragma once

#include "Actor.h"
#include "PlayerController.h"
#include "Model.h"

class Player : public Actor
{
public:
	Player();
	~Player() override = default;

	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

	void SetController(std::unique_ptr<PlayerController> ctrl) { controller = std::move(ctrl); }
	PlayerController* GetController() const { return controller.get(); }

	Model* GetModel() const { return model.get(); }

	void SetSpineAngleX(float angleX) { spineAngleX = angleX; }
	float GetSpinAngleX() const { return spineAngleX; }

	void SetFirstPerson(bool firstPerson) { isFirstPerson = firstPerson; }
	bool IsFirstPerson() const { return isFirstPerson; }

protected:
	std::unique_ptr<PlayerController> controller;
	std::shared_ptr<Model> model = nullptr;

	Animator* anim = nullptr;
	CharacterController* cc = nullptr;

	bool isFirstPerson = false;
	float spineAngleX = 0.0f;
	const Vector2 idleSpineAngle = {0.8f, 0};
	const Vector2 readySpineAngle = {-0.25f, -0.38f};

	float verticalVelocity = 0.0f; // 重力
	float hp = 100.0f;
	float speed = 5.0f;

	ShaderParamListWithMaterialName shaderParamWithMaterialName;
};
