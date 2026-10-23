#include "Gameplay/Actor/Entity.h"
#include "imgui.h"
#include "Application/Time/GameTime.h"

using std::max;
using std::min;

void Entity::Cooldowns::Update()
{
    damageCooldown.Update(Game::Time::unscaledDeltaTime);
}

void Entity::SpawnBloodParticles(const Vector3& position, const Vector3& normal, float scale)
{

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

    if (damageData.knockBackPower > 0 && damageData.hitPosition.has_value())
    {
        Vector3 dir = damageData.hitPosition.value() - transform.position;
        dir.y = 0;
        dir.Normalize();

        AddKnockBack(-dir * damageData.knockBackPower);
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
