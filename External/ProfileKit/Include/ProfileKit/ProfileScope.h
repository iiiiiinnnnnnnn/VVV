#pragma once

#include <cstdint>

//#define USE_PROFILER

class ProfileScope
{
public:
	ProfileScope(const char* name);
	~ProfileScope();
	ProfileScope(const ProfileScope&) = delete;
	ProfileScope& operator=(const ProfileScope&) = delete;
	ProfileScope(ProfileScope&&) = delete;
	ProfileScope& operator=(ProfileScope&&) = delete;

private:
	const char* name = nullptr;
	std::uint64_t generation = 0;
	std::uint64_t startTicks = 0;
	std::uint32_t depth = 0;
	std::uint32_t thread = 0;
	inline static thread_local std::uint32_t nestingDepth = 0;
};

#if defined(USE_PROFILER)

#define PROFILE_DETAIL_CONCAT_IMPL(a, b) a##b
#define PROFILE_DETAIL_CONCAT(a, b) PROFILE_DETAIL_CONCAT_IMPL(a, b)
#define PROFILE_SCOPE(name) ProfileScope PROFILE_DETAIL_CONCAT(profileScope, __COUNTER__)(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)

// 対になるマクロでスコープを作る。途中のreturnでもデストラクタが呼ばれる
#define START_PROFILER(name) { PROFILE_SCOPE(name)
#define END_PROFILER() }

#else

#define PROFILE_SCOPE(name)
#define PROFILE_FUNCTION()
#define START_PROFILER(name) {
#define END_PROFILER() }

#endif