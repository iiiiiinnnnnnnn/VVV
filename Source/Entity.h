// Entity.h

#pragma once
#include "Actor.h"

class Entity : public Actor
{
public:
    Entity(std::string name = "", std::string tag = "", bool isActive = true, int layer = Layer::Default, float life = 100.0f, float maxLife = 100.0f)
        : Actor(name, tag, isActive, layer), life(life), maxLife(maxLife) {}

	void OnDrawGUI() override;

    virtual void OnUpdate() override;

    // HP操作
    void TakeDamage(float damage);
    void Heal(float amount);
    bool IsDead() const { return life <= 0.0f; }

    float GetLife()    const { return life; }
    float GetMaxLife() const { return maxLife; }

protected:
    // 死亡・ダメージ時にオーバーライドして使う
    virtual void OnDead() {}
    virtual void OnDamaged(float damage) {}

    float life    = 100.0f;
    float maxLife = 100.0f;
    struct Cooldowns
    {
        float damageCooldown = 0.0f;
        void Update();
    } cooldowns;
};