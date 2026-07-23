// DebugLog.cpp

#include "Application/SettingsAndDebug/DebugLog.h"

#if defined(_DEBUG)
#include <cstdio>
#include <ctime>
#include <filesystem>
#endif

void DebugLog::Initialize()
{
#if defined(_DEBUG)
	std::filesystem::create_directories("Log");

	std::time_t now = std::time(nullptr);
	std::tm localTime{};
	localtime_s(&localTime, &now);

	char filename[64]{};
	std::strftime(filename, sizeof(filename), "Log/%Y_%m_%d_%H_%M_%S.log", &localTime);

	FILE* stream = nullptr;
	freopen_s(&stream, filename, "w", stdout);
	freopen_s(&stream, filename, "a", stderr);
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::setvbuf(stderr, nullptr, _IONBF, 0);
#endif
}

void DebugLog::Finalize()
{
#if defined(_DEBUG)
	std::fflush(stdout);
	std::fflush(stderr);
#endif
}
