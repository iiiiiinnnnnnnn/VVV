#include "Gameplay/Actor/Entity.h"
#include "imgui.h"
#include "Application/Time/GameTime.h"
#include "Physics/Core/PhysicsComponent.h"
#include <cmath>

using std::max;
using std::min;

void Entity::Cooldowns::Update()
{
    damageCooldown.Update(Game::Time::unscaledDeltaTime);
}

void Entity::DeathOfFall()
{
	// 落下死
	TakeDamage({9999.0f});
}

void Entity::OnDrawGUI()
{
    if (ImGui::TreeNode("Entity Info"))
    {
        ImGui::Text((const char*)u8"ライフ: %.1f / %.1f", life, maxLife);
		ImGui::Text((const char*)u8"ダメージクールダウン: %.2f", cooldowns.damageCooldown.GetRemaining());
		ImGui::Text((const char*)u8"死亡: %s", IsDead() ? "Yes" : "No");
        ImGui::TreePop();
    }
}

void Entity::OnUpdate()
{
	cooldowns.Update();

	// ノックバック
	knockBackVelocity *= powf(0.01f, Game::Time::deltaTime);
	if (knockBackVelocity.LengthSquared() < 0.01f) knockBackVelocity = Vector3::Zero;

	if (transform.position.y < deathOfFallHeight) DeathOfFall();
}

// ダメージを受ける
void Entity::TakeDamage(const DamageData& damageData)
{
    if (!damageData.isForceDamage && cooldowns.damageCooldown.GetRemaining() > 0.0f) return;
    if (IsDead()) return;

    // 攻撃側が指定した強さで、攻撃者から離れる方向へ押す。
    if (std::isfinite(damageData.knockBackPower) && damageData.knockBackPower > 0.0f)
    {
        const Actor* attacker = damageData.attackerCollider
            ? dynamic_cast<Actor*>(damageData.attackerCollider->GetOwner()) : nullptr;
        Vector3 direction = attacker ? transform.position - attacker->transform.position : Vector3::Zero;
        direction.y = 0.0f;
        if (direction.LengthSquared() <= eps && damageData.hitPosition)
        {
            direction = transform.position - *damageData.hitPosition;
            direction.y = 0.0f;
        }
        if (direction.LengthSquared() <= eps && damageData.hitNormal)
        {
            direction = *damageData.hitNormal;
            direction.y = 0.0f;
        }
        if (direction.LengthSquared() <= eps) direction = -transform.GetForward();
        direction.Normalize();
        AddKnockBack(direction * damageData.knockBackPower);
    }

    life -= damageData.damage;
    life = max(life, 0.0f);

    cooldowns.damageCooldown.Reset(cooldowns.DamageCooldownDuration);

    OnDamaged(damageData);

    if (IsDead())
        OnDead(damageData);
}

// 回復する
void Entity::Heal(float amount)
{
	if (life >= maxLife) return;
    life = min(life + amount, maxLife);
	OnHeal(amount);
}
