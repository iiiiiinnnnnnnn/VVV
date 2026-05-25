// Weapon.h

#pragma once

#include "Common.h"
#include "Components.h"
#include "Actor.h"

class Character;

class Weapon : public Actor
{
public:
	Weapon(Character* owner);
	~Weapon() = default;
	void OnUpdate(float elapsedTime) override;
	void OnLateUpdate(float elapsedTime) override;
	void OnRender(const RenderContext& rc, float elapsedTime) override;
	void OnDrawGUI(float elapsedTime) override;

    enum class WeaponType {
        // Pistol
        Colt1911, Luger, Nambu, Tokarev, Webley,
        // SMG
        Greasgun, Mp40, Ppsh41, Sten, Thompson, Type100,
        // Rifle
        Kar98k, Arisaka, G43, GarandM1, MosinNagant, M1Carbine, Springfield, Svt40,
        // AR/LMG
        Bar, Bren, Dp28, Fg42, Stg44, Type99,
        // HMG
        Cal30, Mg42,
        // Heavy
        Bazooka, Panzerschreck, Piat, Flamethrower,
        // Melee
        Knife,

        EnumCount
    };

	static std::string GetModel(WeaponType type);

private:
	Character* owner = nullptr;
	uint32_t ammo = 30;
};
