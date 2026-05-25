// Character.h

#pragma once

#include "Actor.h"
#include "PlayerController.h"
#include "Model.h"
#include "Weapon.h"
#include <PBRShader.h>

class Character : public Actor
{
public:
	enum class Country
	{
		Japan,
		US,
		German,
		Soviet,
		British
	};

	enum class SkinParts : uint32_t
	{
		// ëÃ
		Body_Medic = (1 << 1),			// body_medic ã~ã}

		// ì™
		Head = (1 << 2),				// head ì™
		Head_SoldierB = (1 << 3),		// head_soldier_B ï∫émB
		Head_SoldierA = (1 << 4),		// head_soldier_A ï∫émA
		Head_Brass = (1 << 5),			// head_brass è´çZ
		Head_Officer = (1 << 6),		// head_officer è´çZ
		Head_Medic = (1 << 7),			// head_medic ã~ã}
		Head_GasMask = (1 << 8),		// head_gasmask ÉKÉXÉ}ÉXÉN

		// ëïîı
		Equip_Infantry = (1 << 9),		// equip_infantry ï‡ï∫
		Equip_Medic = (1 << 10),		// equip_medic ã~ã}
	};

	Character(std::string name = "", std::string tag = "", bool isActive = true, std::string layer = "", Country country = Country::Japan, SkinParts skinParts = SkinParts::Head);
	~Character() override = default;

	void OnUpdate(float elapsedTime) override;
	void OnLateUpdate(float elapsedTime) override;
	void OnRender(const RenderContext& rc, float elapsedTime) override;
	void OnDrawGUI(float elapsedTime) override;

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

	void Print();

	static std::string GetModel(Country type);

protected:
	std::unique_ptr<PlayerController> controller;
	std::shared_ptr<Model> model = nullptr;
	std::shared_ptr<Weapon> weapon = nullptr;

	Animator* anim = nullptr;
	Model::Node* handNode = nullptr;
	CharacterController* cc = nullptr;

	bool isFirstPerson = false;
	float spineAngleX = 0.0f;
	const Vector2 idleSpineAngle = {0.8f, 0};
	const Vector2 readySpineAngle = {-0.25f, -0.38f};

	float verticalVelocity = 0.0f; // èdóÕ
	float hp = 100.0f;
	float speed = 5.0f;

	Country country;
	PBRShader::PBRData pbrData;
};
