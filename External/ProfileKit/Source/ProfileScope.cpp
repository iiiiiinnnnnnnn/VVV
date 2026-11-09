#include <ProfileKit/ProfileScope.h>
#include <ProfileKit/Profiler.h>
#include <windows.h>

ProfileScope::ProfileScope(const char* name) : name(name)
{
	if (!name || !*name) return;
	generation = Profiler::captureGeneration.load(std::memory_order_acquire);
	if (!generation) return;
	depth = nestingDepth++;
	thread = GetCurrentThreadId();
	LARGE_INTEGER ticks{};
	QueryPerformanceCounter(&ticks);
	startTicks = ticks.QuadPart;
}

ProfileScope::~ProfileScope()
{
	if (!generation) return;
	LARGE_INTEGER ticks{};
	QueryPerformanceCounter(&ticks);
	--nestingDepth;
	Profiler::RecordScope(name, startTicks, ticks.QuadPart, depth, thread, generation);
}
