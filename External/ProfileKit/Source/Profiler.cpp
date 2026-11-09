#include <ProfileKit/Profiler.h>
#include <windows.h>
#include <algorithm>
#include <cmath>

void Profiler::BeginFrame()
{
	const bool capture = backend.beginFrame && backend.beginFrame();
	const std::lock_guard lock(mutex);
	if (!captureGeneration.load(std::memory_order_relaxed))
		captureGeneration.store(1, std::memory_order_release);
	frameActive = true;
	traceEnabled = capture;
}

void Profiler::EndFrame()
{
	std::vector<TraceEvent> events;
	{
		const std::lock_guard lock(mutex);
		if (!frameActive) return;
		currentFrame.swap(completedFrame);
		currentFrame.clear();
		++frameCount;
		frameActive = false;
		events.swap(traceEvents);
		lastDroppedEvents = droppedEvents;
		droppedEvents = 0;
	}
	if (backend.endFrame) backend.endFrame(events);
	events.clear();
	const std::lock_guard lock(mutex);
	traceEvents.swap(events);
}

void Profiler::DrawGUI()
{
	if (backend.drawGUI) backend.drawGUI();
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
	if (!frameActive || !generation || generation != captureGeneration.load(std::memory_order_relaxed)) return;

	const auto entry = currentFrame.try_emplace(name).first;
	auto& sample = entry->second;
	sample.totalMilliseconds += milliseconds;
	sample.maxMilliseconds = std::max(sample.maxMilliseconds, milliseconds);
	++sample.callCount;
	if (!traceEnabled) return;
	constexpr size_t maxEventsPerFrame = 16384;
	if (traceEvents.size() >= maxEventsPerFrame) { ++droppedEvents; return; }
	traceEvents.push_back({entry->first, begin, end, depth, thread});
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

