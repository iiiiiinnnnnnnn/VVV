// Entity.cpp

#include "Entity.h"
#include "imgui.h"
#include "GameTime.h"

const float Entity::Cooldowns::DamageCooldownDuration = 0.2f;

void Entity::Cooldowns::Update()
{
    if (damageCooldown >= 0.0f)
        damageCooldown -= Game::Time::deltaTime;
}

void Entity::OnDrawGUI()
{
    if (ImGui::TreeNode("Entity Info"))
    {
        ImGui::Text("Life: %.1f / %.1f", life, maxLife);
        ImGui::Text("Damage Cooldown: %.2f", cooldowns.damageCooldown);
        ImGui::Text("IsDead: %s", IsDead() ? "Yes" : "No");
        ImGui::TreePop();
    }
}

void Entity::OnUpdate()
{
    cooldowns.Update();

    // ノックバック
    #if 1
    {
        knockBackVelocity *= powf(0.01f, Game::Time::deltaTime);
        if (knockBackVelocity.LengthSquared() < 0.01f)
            knockBackVelocity = Vector3::Zero;
    }
    #endif

    #if 0 // 落下死
    {
        if (transform.position.y < -50.0f)
            TakeDamage(9999.0f, {});
    }
    #endif
}

// ダメージを受ける
void Entity::TakeDamage(const DamageData& damageData)
{
    if (cooldowns.damageCooldown > 0.0f) return;
    if (IsDead()) return;

    if (damageData.knockBackPower > 0 && damageData.hitPosition.has_value())
    {
        Vector3 dir = damageData.hitPosition.value() - transform.position;
        dir.y = 0;
        dir.Normalize();

        AddKnockBack(-dir * damageData.knockBackPower);
    }

    life -= damageData.damage;
    life = std::max(life, 0.0f);

    cooldowns.damageCooldown = Cooldowns::DamageCooldownDuration;

    OnDamaged(damageData);

    if (IsDead())
        OnDead();
}

// 回復する
void Entity::Heal(float amount)
{
    life = std::min(life + amount, maxLife);
}