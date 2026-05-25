// Character.h

#pragma once

#include "Actor.h"
#include "PlayerController.h"
#include "Model.h"
#include "Weapon.h"

class Character : public Actor {
public:
	using Actor::Actor;
	~Character() override = default;

	void InitCharacter();
	void OnUpdate(float elapsedTime) override;
	void OnLateUpdate(float elapsedTime) override;
	void OnRender(const RenderContext& rc, float elapsedTime) override;
	void OnDrawGUI(float elapsedTime) override;

	enum class SkinParts : uint32_t {
		// ëÃ
		Body_Medic = (1 << 1),  // Japan_body_medic

		// ì™
		Head = (1 << 2),  // Japan_head
		Head_SoldierB = (1 << 3),  // Japan_head_soldier_B
		Head_SoldierA = (1 << 4),  // Japan_head_soldier_A
		Head_Brass = (1 << 5),  // Japan_head_brass
		Head_Officer = (1 << 6),  // Japan_head_officer
		Head_Medic = (1 << 7),  // Japan_head_medic
		Head_GasMask = (1 << 8),  // Japan_head_gasmask

		// ëïîı
		Equip_Infantry = (1 << 9),  // Japan_equip_infantry
		Equip_Medic = (1 << 10), // Japan_equip_medic
	};
	void SetSkin(uint32_t parts);

    void SetController(std::unique_ptr<PlayerController> ctrl) { controller = std::move(ctrl); }
    PlayerController* GetController() const { return controller.get(); }

    Model::Node* GetHandNode() const { return handNode; }

    void SetWeapon(std::shared_ptr<Weapon> weapon) { this->weapon = weapon; }
    Weapon* GetWeapon() const { return weapon.get(); }

    Model* GetModel() const { return model.get(); }

	void SetSpineAngleX(float angleX) { spineAngleX = angleX; }
	float GetSpinAngleX() const { return spineAngleX; }

	void SetFirstPerson(bool firstPerson) { isFirstPerson = firstPerson; }
	bool IsFirstPerson() const { return isFirstPerson; }

protected:
    std::unique_ptr<PlayerController> controller;
    std::shared_ptr<Model> model = nullptr;
    std::shared_ptr<Weapon> weapon = nullptr;
	Animator* anim = nullptr;
	Model::Node* handNode = nullptr;
	CharacterController* cc = nullptr;

	bool isFirstPerson = false;
	float spineAngleX = 0.0f;
	const Vector2 idleSpineAngle = { 0.8f, 0 };
	const Vector2 readySpineAngle = { -0.25f, -0.38f };

	float verticalVelocity = 0.0f; // èdóÕ
    float hp = 100.0f;
    float speed = 5.0f;
};
