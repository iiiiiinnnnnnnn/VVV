#pragma once

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>

#include "Application/SettingsAndDebug/ProfileScope.h"
#include <string>
#include <unordered_map>

class Profiler
{
public:
	struct Sample
	{
		double totalMilliseconds = 0.0;
		double maxMilliseconds = 0.0;
		std::uint64_t callCount = 0;
	};

	using Samples = std::unordered_map<std::string, Sample>;

	// 計測
	static void BeginFrame();
	static void EndFrame();
	static void AddSample(const char* name, double milliseconds);
	static void Clear();
	static const Samples& GetResults();
	static std::uint64_t GetFrameCount();
	static void DrawGUI();

private:
	friend class ProfileScope;
	struct TraceEvent
	{
		const char* name = nullptr;
		std::uint64_t begin = 0;
		std::uint64_t end = 0;
		std::uint32_t depth = 0;
		std::uint32_t thread = 0;
	};
	static void RecordScope(const char* name, std::uint64_t begin, std::uint64_t end,
		std::uint32_t depth, std::uint32_t thread, std::uint64_t generation);

	inline static std::atomic<std::uint64_t> captureGeneration = 0;
	inline static std::vector<TraceEvent> traceEvents;
	inline static std::unordered_map<std::uint32_t, std::uint32_t> timelineTracks;
	inline static std::uint32_t mainThread = 0;
	inline static std::uint32_t timelineFrame = 0;
	inline static std::uint64_t droppedEvents = 0;
	inline static std::uint64_t lastDroppedEvents = 0;
	inline static bool traceEnabled = false;
	inline static std::mutex mutex;
	inline static bool frameActive = false;
	inline static Samples currentFrame;
	inline static Samples completedFrame;
	inline static std::uint64_t frameCount = 0;
};
