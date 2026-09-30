// SoundTracks.generated.h
#pragma once

#include <array>
#include <string_view>

enum class SoundTrack
{
	SE_PLAYER_PL_WALK,
	SE_PLAYER_PL_SWORD,
	SE_VOICE_PL_ATTACK_LONG,
	SE_VOICE_PL_ATTACK_MIDDLE,
	SE_VOICE_PL_ATTACK_SHORT,
	SE_BOSS_GROUND,
	SE_BOSS_ARACORE_VOICE,
	SE_BOSS_IMPACT,
	SE_BOSS_JUMP,
	BGM_CAVE_AMBIENT,
	BGM_CHASE,
	SE_BOSS_DIE,
	SE_CRYSTAL_C_BREAK_LARGE,
	SE_CRYSTAL_C_BREAK_SMALL,
	SE_DEER_DIE,
	SE_DEER_LOOKIN,
	SE_PLAYER_PL_WALK_GRASS,
	SE_PLAYER_PL_WALK_ROCK,
	SE_PLAYER_PL_DODGE,
	SE_PLAYER_PL_JUSTDODGE,
	SE_BOSS_ARACORE_THREAT,
	SE_PLAYER_PL_HIT,
};

struct SoundDefinition
{
	SoundTrack id;
	std::string_view name;
	std::string_view path;
};

inline constexpr std::array SoundDefinitions =
{
	SoundDefinition{SoundTrack::SE_PLAYER_PL_WALK, "SE_PLAYER_PL_WALK", "Resources/Sound/SE/Player/pl_walk.wav"},
	SoundDefinition{SoundTrack::SE_PLAYER_PL_SWORD, "SE_PLAYER_PL_SWORD", "Resources/Sound/SE/Player/pl_sword.wav"},
	SoundDefinition{SoundTrack::SE_VOICE_PL_ATTACK_LONG, "SE_VOICE_PL_ATTACK_LONG", "Resources/Sound/SE/Voice/pl_attack_long.wav"},
	SoundDefinition{SoundTrack::SE_VOICE_PL_ATTACK_MIDDLE, "SE_VOICE_PL_ATTACK_MIDDLE", "Resources/Sound/SE/Voice/pl_attack_middle.wav"},
	SoundDefinition{SoundTrack::SE_VOICE_PL_ATTACK_SHORT, "SE_VOICE_PL_ATTACK_SHORT", "Resources/Sound/SE/Voice/pl_attack_short.wav"},
	SoundDefinition{SoundTrack::SE_BOSS_GROUND, "SE_BOSS_GROUND", "Resources/Sound/SE/Boss/ground.wav"},
	SoundDefinition{SoundTrack::SE_BOSS_ARACORE_VOICE, "SE_BOSS_ARACORE_VOICE", "Resources/Sound/SE/Boss/aracore_voice.wav"},
	SoundDefinition{SoundTrack::SE_BOSS_IMPACT, "SE_BOSS_IMPACT", "Resources/Sound/SE/Boss/impact.wav"},
	SoundDefinition{SoundTrack::SE_BOSS_JUMP, "SE_BOSS_JUMP", "Resources/Sound/SE/Boss/jump.wav"},
	SoundDefinition{SoundTrack::BGM_CAVE_AMBIENT, "BGM_CAVE_AMBIENT", "Resources/Sound/BGM/cave_ambient.wav"},
	SoundDefinition{SoundTrack::BGM_CHASE, "BGM_CHASE", "Resources/Sound/BGM/chase.wav"},
	SoundDefinition{SoundTrack::SE_BOSS_DIE, "SE_BOSS_DIE", "Resources/Sound/SE/Boss/die.wav"},
	SoundDefinition{SoundTrack::SE_CRYSTAL_C_BREAK_LARGE, "SE_CRYSTAL_C_BREAK_LARGE", "Resources/Sound/SE/Crystal/c_break_large.wav"},
	SoundDefinition{SoundTrack::SE_CRYSTAL_C_BREAK_SMALL, "SE_CRYSTAL_C_BREAK_SMALL", "Resources/Sound/SE/Crystal/c_break_small.wav"},
	SoundDefinition{SoundTrack::SE_DEER_DIE, "SE_DEER_DIE", "Resources/Sound/SE/Deer/die.wav"},
	SoundDefinition{SoundTrack::SE_DEER_LOOKIN, "SE_DEER_LOOKIN", "Resources/Sound/SE/Deer/lookin.wav"},
	SoundDefinition{SoundTrack::SE_PLAYER_PL_WALK_GRASS, "SE_PLAYER_PL_WALK_GRASS", "Resources/Sound/SE/Player/pl_walk_grass.wav"},
	SoundDefinition{SoundTrack::SE_PLAYER_PL_WALK_ROCK, "SE_PLAYER_PL_WALK_ROCK", "Resources/Sound/SE/Player/pl_walk_rock.wav"},
	SoundDefinition{SoundTrack::SE_PLAYER_PL_DODGE, "SE_PLAYER_PL_DODGE", "Resources/Sound/SE/Player/pl_dodge.wav"},
	SoundDefinition{SoundTrack::SE_PLAYER_PL_JUSTDODGE, "SE_PLAYER_PL_JUSTDODGE", "Resources/Sound/SE/Player/pl_justdodge.wav"},
	SoundDefinition{SoundTrack::SE_BOSS_ARACORE_THREAT, "SE_BOSS_ARACORE_THREAT", "Resources/Sound/SE/Boss/aracore_threat.wav"},
	SoundDefinition{SoundTrack::SE_PLAYER_PL_HIT, "SE_PLAYER_PL_HIT", "Resources/Sound/SE/Player/pl_hit.wav"},
};

constexpr const SoundDefinition* FindSoundDefinition(SoundTrack id)
{
	for (const auto& sound : SoundDefinitions)
		if (sound.id == id) return &sound;
	return nullptr;
}

constexpr const SoundDefinition* FindSoundDefinition(std::string_view name)
{
	for (const auto& sound : SoundDefinitions)
		if (sound.name == name) return &sound;
	return nullptr;
}

constexpr std::string_view SoundTrackName(SoundTrack id)
{
	const auto* sound = FindSoundDefinition(id);
	return sound ? sound->name : std::string_view{};
}
