// Entity.h

#pragma once
#include "Actor.h"

class KnockBackData
{
public:
	KnockBackData() = default;
	KnockBackData(class Entity* source, float power) : hasData(true), source(source), power(power) {}
	bool HasData() const { return hasData; }
	class Entity* GetSource() const { return source; }
	float GetPower() const { return power; }
private:
    bool hasData = false;
    class Entity* source = nullptr;
    float power = false;
};

class Entity : public Actor
{
public:
    Entity(std::string name = "", std::string tag = "", bool isActive = true, int layer = Layer::Default, float life = 100.0f, float maxLife = 100.0f)
        : Actor(name, tag, isActive, layer), life(life), maxLife(maxLife) {}

	void OnDrawGUI() override;

    virtual void OnUpdate() override;

    // HP操作
    void TakeDamage(float damage, KnockBackData knockBackData = {});
    void Heal(float amount);
    bool IsDead() const { return life <= 0.0f; }

    float GetLife()    const { return life; }
    float GetMaxLife() const { return maxLife; }

    Vector3 GetKnockBackVelocity() const { return knockBackVelocity; }
    void AddKnockBack(const Vector3& velocity) { knockBackVelocity += velocity; }

protected:
    // 死亡・ダメージ時にオーバーライドして使う
    virtual void OnDead() {}
    virtual void OnDamaged(float damage, KnockBackData knockBackData) {}

    Vector3 knockBackVelocity = Vector3::Zero;

    float life    = 100.0f;
    float maxLife = 100.0f;
    struct Cooldowns
    {
        float damageCooldown = 0.0f;
        void Update();
    } cooldowns;
};