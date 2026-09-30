// Effects.generated.h
#pragma once

#include <array>
#include <string_view>

enum class EffectId
{
	CRITICAL,
	CRYSTAL_BREAK,
	CRYSTAL_BREAK2,
	ENEMY_DEAD,
	GUARD_COMMON,
	HEAL,
	PLAYER_QUIT,
	SMOKE,
	SWORD_TRAIL,
	WIND,
};

struct EffectDefinition
{
	EffectId id;
	std::string_view name;
	std::string_view path;
};

inline constexpr std::array EffectDefinitions =
{
	EffectDefinition{EffectId::CRITICAL, "CRITICAL", "Resources/Effect/critical.efkpkg"},
	EffectDefinition{EffectId::CRYSTAL_BREAK, "CRYSTAL_BREAK", "Resources/Effect/crystal_break.efkpkg"},
	EffectDefinition{EffectId::CRYSTAL_BREAK2, "CRYSTAL_BREAK2", "Resources/Effect/crystal_break2.efkpkg"},
	EffectDefinition{EffectId::ENEMY_DEAD, "ENEMY_DEAD", "Resources/Effect/enemy_dead.efkpkg"},
	EffectDefinition{EffectId::GUARD_COMMON, "GUARD_COMMON", "Resources/Effect/guard_common.efkpkg"},
	EffectDefinition{EffectId::HEAL, "HEAL", "Resources/Effect/heal.efkpkg"},
	EffectDefinition{EffectId::PLAYER_QUIT, "PLAYER_QUIT", "Resources/Effect/player_quit.efkpkg"},
	EffectDefinition{EffectId::SMOKE, "SMOKE", "Resources/Effect/smoke.efkpkg"},
	EffectDefinition{EffectId::SWORD_TRAIL, "SWORD_TRAIL", "Resources/Effect/sword_trail.efkpkg"},
	EffectDefinition{EffectId::WIND, "WIND", "Resources/Effect/wind.efkpkg"},
};

constexpr const EffectDefinition* FindEffectDefinition(EffectId id)
{
	for (const auto& effect : EffectDefinitions)
		if (effect.id == id) return &effect;
	return nullptr;
}

constexpr const EffectDefinition* FindEffectDefinition(std::string_view name)
{
	for (const auto& effect : EffectDefinitions)
		if (effect.name == name) return &effect;
	return nullptr;
}
