// Entity.cpp

#include "Entity.h"
#include "imgui.h"
#include "GameTime.h"

void Entity::Cooldowns::Update()
{
    if (damageCooldown >= 0.0f)
        damageCooldown -= Game::Time::deltaTime;
}

void Entity::OnDrawGUI()
{
    ImGui::Text("Life: %.1f / %.1f", life, maxLife);
}

void Entity::OnUpdate()
{
    cooldowns.Update();

    knockBackVelocity *= powf(0.01f, Game::Time::deltaTime);

    if (knockBackVelocity.LengthSquared() < 0.01f)
        knockBackVelocity = Vector3::Zero;
}

void Entity::TakeDamage(float damage, KnockBackData knockBackData)
{
    if (cooldowns.damageCooldown > 0.0f) return;
    if (IsDead()) return;

    if (knockBackData.HasData())
    {
        Vector3 dir = knockBackData.GetSource()->transform.position - transform.position;
        dir.y = 0;
        dir.Normalize();

        AddKnockBack(-dir * knockBackData.GetPower());
    }

    life -= damage;
    life = max(life, 0.0f);

    cooldowns.damageCooldown = 0.6f;

    OnDamaged(damage, knockBackData);

    if (IsDead())
        OnDead();
}

void Entity::Heal(float amount)
{
    life = min(life + amount, maxLife);
}