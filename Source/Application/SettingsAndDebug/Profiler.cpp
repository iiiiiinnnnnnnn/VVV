#include "Application/SettingsAndDebug/Profiler.h"
#include "TimelineProfiler/Profiler.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string_view>

void Profiler::BeginFrame()
{
	auto& timeline = TimelineProfiler::gProfiler;
	if (!timeline.IsInitialized())
	{
		timeline.Initialize(60);
		timeline.RegisterCurrentThread("Main Thread");
		mainThread = GetCurrentThreadId();
		timelineTracks.emplace(mainThread, 0);
		captureGeneration.store(1, std::memory_order_release);
	}
	timeline.Tick();
	const std::lock_guard lock(mutex);
	frameActive = true;
	timelineFrame = timeline.GetFrameRange().End;
	traceEnabled = !timeline.IsPaused();
}

void Profiler::EndFrame()
{
	const std::lock_guard lock(mutex);
	if (!frameActive) return;
	currentFrame.swap(completedFrame);
	currentFrame.clear();
	++frameCount;
	frameActive = false;

	// ワーカースレッドはここへ直接触れず、記録の公開はメインスレッドで行う。
	if (traceEnabled)
	{
		auto& timeline = TimelineProfiler::gProfiler;
		for (const auto& event : traceEvents)
		{
			auto track = timelineTracks.find(event.thread);
			if (track == timelineTracks.end())
			{
				const std::string name = "Thread " + std::to_string(event.thread);
				const auto index = timeline.RegisterTrack(name.c_str(),
					TimelineProfiler::Profiler::EventTrack::EType::CPU, event.thread);
				track = timelineTracks.emplace(event.thread, index).first;
			}
			TimelineProfiler::ProfilerEvent sample{};
			sample.pName = event.name;
			sample.pFilePath = "";
			sample.TicksBegin = event.begin;
			sample.TicksEnd = event.end;
			sample.Depth = std::min(event.depth + (event.thread == mainThread ? 1u : 0u), 31u);
			sample.ThreadIndex = track->second;
			sample.Color = ImGui::ColorConvertFloat4ToU32(ImColor::HSV(
				static_cast<float>(std::hash<std::string_view>{}(event.name) % 360) / 360.0f, 0.5f, 0.65f).Value);
			timeline.AddEvent(track->second, sample, timelineFrame);
		}
	}
	traceEvents.clear();
	lastDroppedEvents = droppedEvents;
	droppedEvents = 0;
}

void Profiler::AddSample(const char* name, double milliseconds)
{
	if (!name || !*name || !std::isfinite(milliseconds) || milliseconds < 0.0) return;
	const std::lock_guard lock(mutex);
	if (!frameActive) return;
	auto& sample = currentFrame[name];
	sample.totalMilliseconds += milliseconds;
	sample.maxMilliseconds = std::max(sample.maxMilliseconds, milliseconds);
	++sample.callCount;
}

void Profiler::RecordScope(const char* name, std::uint64_t begin, std::uint64_t end,
	std::uint32_t depth, std::uint32_t thread, std::uint64_t generation)
{
	if (!name || !*name || end < begin) return;
	static const double ticksToMilliseconds = [] {
		LARGE_INTEGER frequency{};
		QueryPerformanceFrequency(&frequency);
		return 1000.0 / frequency.QuadPart;
	}();
	const double milliseconds = (end - begin) * ticksToMilliseconds;
	const std::lock_guard lock(mutex);
	if (!generation || generation != captureGeneration.load(std::memory_order_relaxed)) return;

	const auto entry = currentFrame.try_emplace(name).first;
	auto& sample = entry->second;
	sample.totalMilliseconds += milliseconds;
	sample.maxMilliseconds = std::max(sample.maxMilliseconds, milliseconds);
	++sample.callCount;
	if (!traceEnabled) return;
	constexpr size_t maxEventsPerFrame = 16384;
	if (traceEvents.size() >= maxEventsPerFrame) { ++droppedEvents; return; }
	traceEvents.push_back({entry->first.c_str(), begin, end, depth, thread});
}

void Profiler::Clear()
{
	const std::lock_guard lock(mutex);
	traceEvents.clear();
	currentFrame.clear();
	completedFrame.clear();
	frameCount = 0;
	droppedEvents = lastDroppedEvents = 0;
	if (captureGeneration.load(std::memory_order_relaxed))
		captureGeneration.fetch_add(1, std::memory_order_release);
}

const Profiler::Samples& Profiler::GetResults()
{
	return completedFrame;
}

std::uint64_t Profiler::GetFrameCount()
{
	return frameCount;
}

void Profiler::DrawGUI()
{
	if (!TimelineProfiler::gProfiler.IsInitialized()) return;
	ImGui::TextUnformatted((const char*)u8"CPU実測タイムライン（親区間の時間には子区間を含みます）");
	ImGui::TextUnformatted((const char*)u8"Ctrl＋ホイール：拡大縮小 / 右ドラッグ：移動 / バーをクリック：詳細");
	if (lastDroppedEvents)
		ImGui::Text((const char*)u8"記録上限により省略したイベント：%llu", static_cast<unsigned long long>(lastDroppedEvents));
	ImGui::Separator();
	TimelineProfiler::DrawProfilerHUD();
}
