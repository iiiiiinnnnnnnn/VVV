// Entity.cpp

#include "Entity.h"
#include "imgui.h"

void Entity::OnDrawGUI()
{
    ImGui::Text("Life: %.1f / %.1f", life, maxLife);
}

void Entity::TakeDamage(float damage)
{
    if (IsDead()) return;
    life -= damage;
    life  = max(life, 0.0f);
    OnDamaged(damage);
    if (IsDead()) OnDead();
}

void Entity::Heal(float amount)
{
    life = min(life + amount, maxLife);
}