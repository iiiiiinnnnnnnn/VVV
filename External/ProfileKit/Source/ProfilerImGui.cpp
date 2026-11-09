#include <ProfileKit/Profiler.h>
#include <ProfileKit/ProfilerTimeline.h>
#include "TimelineProfiler/Profiler.h"
#include <imgui.h>
#include <algorithm>
#include <string_view>
#include <windows.h>

class ProfilerTimelineBackend
{
public:
	static bool BeginFrame()
	{
		auto& timeline = TimelineProfiler::gProfiler;
		if (!timeline.IsInitialized())
		{
			timeline.Initialize(60);
			timeline.RegisterCurrentThread("Main Thread");
			mainThread = GetCurrentThreadId();
			tracks.emplace(mainThread, 0);
		}
		timeline.Tick();
		frame = timeline.GetFrameRange().End;
		return !timeline.IsPaused();
	}

	static void EndFrame(std::span<const Profiler::TraceEvent> events)
	{
		auto& timeline = TimelineProfiler::gProfiler;
		for (const auto& event : events)
		{
			auto track = tracks.find(event.thread);
			if (track == tracks.end())
			{
				const std::string name = "Thread " + std::to_string(event.thread);
				const auto index = timeline.RegisterTrack(name.c_str(),
					TimelineProfiler::Profiler::EventTrack::EType::CPU, event.thread);
				track = tracks.emplace(event.thread, index).first;
			}
			TimelineProfiler::ProfilerEvent sample{};
			sample.pName = event.name.c_str();
			sample.pFilePath = "";
			sample.TicksBegin = event.begin;
			sample.TicksEnd = event.end;
			sample.Depth = std::min(event.depth + (event.thread == mainThread ? 1u : 0u), 31u);
			sample.ThreadIndex = track->second;
			sample.Color = ImGui::ColorConvertFloat4ToU32(ImColor::HSV(
				static_cast<float>(std::hash<std::string_view>{}(event.name) % 360) / 360.0f, 0.5f, 0.65f).Value);
			timeline.AddEvent(track->second, sample, frame);
		}
	}

	static void DrawGUI()
	{
		if (!TimelineProfiler::gProfiler.IsInitialized()) return;
		TimelineProfiler::DrawProfilerHUD();
	}

private:
	inline static std::unordered_map<std::uint32_t, std::uint32_t> tracks;
	inline static std::uint32_t mainThread = 0;
	inline static std::uint32_t frame = 0;
};

Profiler::Backend CreateProfilerTimelineBackend()
{
	return {ProfilerTimelineBackend::BeginFrame,
		ProfilerTimelineBackend::EndFrame, ProfilerTimelineBackend::DrawGUI};
}
