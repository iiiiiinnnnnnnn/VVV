// Entity.h

#pragma once
#include <optional>
#include <string>
#include "Gameplay/Stage/Stage.h"
#include "Gameplay/Actor/Actor.h"

class PhysicsComponent;

struct DamageData
{
    float damage = 0.0f;
    float knockBackPower = 0.0f;

	PhysicsComponent* hitColliderSelf = nullptr;
    PhysicsComponent* hitColliderOther = nullptr;

    std::optional<Vector3> hitPosition;
	std::optional<Vector3> hitNormal;
};

class Entity : public Actor
{
public:
    Entity(std::string name = "", std::string tag = "", bool isActive = true, float life = 100.0f, float maxLife = 100.0f)
        : Actor(name, tag, isActive), life(life), maxLife(maxLife) {}

    virtual void OnUpdate() override;

    void TakeDamage(const DamageData& damageData);
    void Heal(float amount);
    bool IsDead() const { return life <= 0.0f; }

    float GetLife() const { return life; }
    float GetMaxLife() const { return maxLife; }

    Vector3 GetKnockBackVelocity() const { return knockBackVelocity; }
    void AddKnockBack(const Vector3& velocity) { knockBackVelocity += velocity; }

	void SpawnBloodParticles(const Vector3& position, const Vector3& normal, float scale = 1.0f);

protected:
    void OnDrawGUI() override;

    virtual void OnDead(const DamageData& damageData) {}
    virtual void OnDamaged(const DamageData& damageData) {}

    Vector3 knockBackVelocity = Vector3::Zero;

    float life = 100.0f;
    float maxLife = 100.0f;

    struct Cooldowns
    {
        static const float DamageCooldownDuration;
        float damageCooldown = 0.0f;
        void Update();
    } cooldowns;
};
