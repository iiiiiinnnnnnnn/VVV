#pragma once

#include "Application/Time/Timer.h"
#include <optional>
#include <string>
#include "Gameplay/Stage/Stage.h"
#include "Gameplay/Actor/Actor.h"

class PhysicsComponent;

struct DamageData
{
    float damage = 0.0f;
    float knockBackPower = 0.0f;
	bool isForceDamage = false; // Bypasses only the damage cooldown.
	bool specialAttack = false;

	PhysicsComponent* attackerCollider = nullptr;
    PhysicsComponent* targetCollider = nullptr;

    std::optional<Vector3> hitPosition;
	std::optional<Vector3> hitNormal;
};

class Entity : public Actor
{
public:
	Entity(std::string name = "", std::string tag = "", bool isActive = true,
		const Transform& transform = {}, float life = 100.0f, float maxLife = 100.0f)
        : Actor(name, tag, isActive, transform), life(life), maxLife(maxLife) {}

    virtual void OnUpdate() override;
	virtual void OnHeal(float amount) {};

    virtual void TakeDamage(const DamageData& damageData);
    void Heal(float amount);
    bool IsDead() const { return life <= 0.0f; }

    float GetLife() const { return life; }
    float GetMaxLife() const { return maxLife; }

    Vector3 GetKnockBackVelocity() const { return knockBackVelocity; }
    void AddKnockBack(const Vector3& velocity) { knockBackVelocity += velocity; }

	void SpawnBloodParticles(const Vector3& position, const Vector3& normal, float scale = 1.0f);

private:
    struct Cooldowns
    {
        float DamageCooldownDuration = 0.05f;
        Game::Timer damageCooldown{0.0f};

        void Update();
    } cooldowns;

public:
    Cooldowns& GetCooldowns()
    {
        return cooldowns;
    }

    const Cooldowns& GetCooldowns() const
    {
        return cooldowns;
    }

protected:
	void OnDrawGUI() override;

	virtual void DeathOfFall();

    virtual void OnDead(const DamageData& damageData) {}
    virtual void OnDamaged(const DamageData& damageData) {}

    Vector3 knockBackVelocity = Vector3::Zero;

    float life = 100.0f;
    float maxLife = 100.0f;
	float deathOfFallHeight = -50.0f;
};
