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
}

void Entity::TakeDamage(float damage)
{
	if (cooldowns.damageCooldown > 0.0f) return; // ダメージクールタイム中は無効
    if (IsDead()) return;

    life -= damage;
    life  = max(life, 0.0f);
    cooldowns.damageCooldown = 0.6f;

    OnDamaged(damage);

    if (IsDead()) OnDead();
}

void Entity::Heal(float amount)
{
    life = min(life + amount, maxLife);
}