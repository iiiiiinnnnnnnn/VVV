// Modified for VEER: CPU-only TimelineProfiler sample, isolated in TimelineProfiler namespace.
#pragma once

#ifndef WITH_PROFILING
#define WITH_PROFILING 1
#endif

#if !WITH_PROFILING

#define PROFILE_REGISTER_THREAD(...)
#define PROFILE_FRAME()
#define PROFILE_EXECUTE_COMMANDLISTS(...)

#define PROFILE_CPU_SCOPE(...)
#define PROFILE_CPU_BEGIN(...)
#define PROFILE_CPU_END()

#define PROFILE_GPU_SCOPE(...)
#define PROFILE_GPU_BEGIN(...)
#define PROFILE_GPU_END()

#define PROFILE_PRESENT(...)

#else

#include <array>
#include <atomic>
#include <cinttypes>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>
#include <queue>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi.h>

namespace TimelineProfiler
{
void DrawProfilerHUD();

void HandleAssertMessage(const char* pMessage);

template<typename... Args>
static void HandleAssertMessage(const char* pExpression, const char* pFileName, int line, const char* pFmt = nullptr, Args... args)
{
	char message[1024]{};
	if (pFmt)
		sprintf_s(message, pFmt, std::forward<Args>(args)...);
	char finalMessage[1024]{};
	sprintf_s(finalMessage, "ASSERT FAILED:\nExpression: %s\nFile: %s:%d\n%s\n", pExpression, __FILE__, __LINE__, message);
	HandleAssertMessage(finalMessage);
}

#define gAssert(op, ...)                                               \
	do                                                                 \
	{                                                                  \
		bool result = (op);                                            \
		if (result == false)                                           \
		{                                                              \
			::TimelineProfiler::HandleAssertMessage(#op, __FILE__, __LINE__, __VA_ARGS__); \
			__debugbreak();                                            \
		}                                                              \
	} while (false)

#define gVerify(op, expected, ...) \
	do                        \
	{                         \
		auto r = (op);        \
		gAssert(r expected, __VA_ARGS__);  \
	} while (false)

#define gVerifyHR(op)				 gVerify(op, == S_OK, "HRESULT Failed")
#define gBoundCheck(val, minV, maxV) gAssert(val >= minV && val < maxV, "Value out of bounds")

#define _STRINGIFY(a)	   #a
#define STRINGIFY(a)	   _STRINGIFY(a)
#define CONCAT_IMPL(x, y)  x##y
#define MACRO_CONCAT(x, y) CONCAT_IMPL(x, y)

// Basic types
using uint64 = uint64_t;
using uint32 = uint32_t;
using uint16 = uint16_t;
using uint8	 = uint8_t;
template <typename T>
using Span = std::span<T>;
template <typename T>
using Array = std::vector<T>;
template <typename T, size_t Size>
using StaticArray = std::array<T, Size>;
template <typename K, typename V>
using HashMap = std::unordered_map<K, V>;
using Mutex = std::mutex;

struct URange
{
	uint32 Begin = 0;
	uint32 End	 = 0;
	uint32 GetLength() const { return End - Begin; }
};

/*
	General
*/

// Usage:
//		PROFILE_REGISTER_THREAD(const char* pName)
//		PROFILE_REGISTER_THREAD()
#define PROFILE_REGISTER_THREAD(...) ::TimelineProfiler::gProfiler.RegisterCurrentThread(__VA_ARGS__)

/// Usage:
//		PROFILE_FRAME()
#define PROFILE_FRAME() ::TimelineProfiler::gProfiler.Tick()

/// Usage:
///		PROFILE_EXECUTE_COMMANDLISTS(ID3D12CommandQueue* pQueue, Span<ID3D12CommandLists*> commandLists)

/*
	CPU Profiling
*/

// Usage:
//		PROFILE_CPU_SCOPE(const char* pName)
//		PROFILE_CPU_SCOPE()
#define PROFILE_CPU_SCOPE(...) ::TimelineProfiler::CPUProfileScope MACRO_CONCAT(profiler, __COUNTER__)(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)

// Usage:
//		PROFILE_CPU_BEGIN(const char* pName)
//		PROFILE_CPU_BEGIN()
#define PROFILE_CPU_BEGIN(...) ::TimelineProfiler::gProfiler.BeginEvent(__VA_ARGS__)
// Usage:
//		PROFILE_CPU_END()
#define PROFILE_CPU_END() ::TimelineProfiler::gProfiler.EndEvent()


#define PROFILE_PRESENT(swapchain)		 ::TimelineProfiler::gProfiler.Present(swapchain)


template <typename T, uint32 N>
struct FixedArray
{
public:
	T& Pop()
	{
		gAssert(Length > 0);
		--Length;
		return Data[Length];
	}

	T& Push()
	{
		Length++;
		gAssert(Length < N);
		return Data[Length - 1];
	}

	T& Top()
	{
		gAssert(Length > 0);
		return Data[Length - 1];
	}

	uint32 GetSize() const { return Length; }

private:
	uint32			  Length = 0;
	StaticArray<T, N> Data{};
};



// Single event
struct ProfilerEvent
{
	const char* pName			 = nullptr;	 ///< Name of event
	const char* pFilePath		 = nullptr;	 ///< File path of location where event was started
	uint32		Color		: 24 = 0xFFFFFF; ///< Color
	uint32		Depth		: 8	 = 0;		 ///< Stack depth of event
	uint32		LineNumber	: 18 = 0;		 ///< Line number of file where event was started
	uint32		ThreadIndex : 10 = 0;		 ///< Index of thread this event is started on
	uint32		QueueIndex	: 4	 = 0;		 ///< GPU Queue Index (GPU-specific)
	uint64		TicksBegin		 = 0;		 ///< Begin CPU ticks
	uint64		TicksEnd		 = 0;		 ///< End CPU ticks

	bool   IsValid() const { return TicksBegin != 0 && TicksEnd != 0; }
	uint32 GetColor() const { return Color | (0xFF << 24); }
};
static_assert(std::has_unique_object_representations_v<ProfilerEvent>);


// Data for a single frame of profiling events
using ProfilerEventData = Array<ProfilerEvent>;


//-----------------------------------------------------------------------------
// [SECTION] GPU Profiler
//-----------------------------------------------------------------------------

class Profiler;
extern Profiler gProfiler;

struct CPUProfilerCallbacks
{
	using EventBeginFn = void (*)(const char* /*pName*/, void* /*pUserData*/);
	using EventEndFn   = void (*)(void* /*pUserData*/);

	EventBeginFn OnEventBegin = nullptr;
	EventEndFn	 OnEventEnd	  = nullptr;
	void*		 pUserData	  = nullptr;
};

// CPU Profiler
// Also responsible for updating GPU profiler
// Also responsible for drawing HUD
class Profiler
{
public:
	void Initialize(uint32 historySize);

	void Shutdown();

	// Start and push an event on the current thread
	void BeginEvent(const char* pName, uint32 color, const char* pFilePath, uint32 lineNumber);

	// Start and push an event on the current thread
	void BeginEvent(const char* pName, uint32 color = 0) { BeginEvent(pName, color, "", 0); }

	// End and pop the last pushed event on the current thread
	void EndEvent();

	void AddEvent(uint32 trackIndex, const ProfilerEvent& event, uint32 frameIndex);

	void Present(IDXGISwapChain* pSwapChain);

	bool IsInitialized() const { return m_IsInitialized; }

	// Resolve the last frame and advance to the next frame.
	// Call at the START of the frame.
	void Tick();

	// Initialize a thread with an optional name
	int RegisterCurrentThread(const char* pName = nullptr);

	struct EventTrack
	{
		enum class EType
		{
			CPU,
			GPU,
			Present,
		};

		ProfilerEventData& GetFrameData(int frameIndex) { return Events[frameIndex % Events.size()]; }
		const ProfilerEventData& GetFrameData(int frameIndex) const { return Events[frameIndex % Events.size()]; }

		char								Name[128]{};			///< Name
		uint32								ID				= 0;	///< ThreadID (or generic identifier)
		uint32								Index			= 0;	///< Index in Tracks Array
		EType								Type			= EType::CPU;
		
		static constexpr int				MAX_STACK_DEPTH = 32;
		FixedArray<uint32, MAX_STACK_DEPTH> EventStack;
		Array<ProfilerEventData>			Events;
	};
	
	// Register a new track
	int RegisterTrack(const char* pName, EventTrack::EType type, uint32 id);

	URange GetFrameRange() const
	{
		uint32 begin = m_FrameIndex - std::min(m_FrameIndex, m_HistorySize) + 1;
		uint32 end	 = m_FrameIndex;
		return { .Begin = begin, .End = end };
	}

	uint64 GetFirstFrameAnchorTicks() const { return m_BeginFrameTicks.empty() ? 0 : m_BeginFrameTicks[GetFrameRange().Begin % m_BeginFrameTicks.size()]; }

	Span<const EventTrack> GetTracks() const { return m_Tracks; }

	void SetEventCallback(const CPUProfilerCallbacks& inCallbacks) { m_EventCallback = inCallbacks; }
	void SetPaused(bool paused) { m_QueuedPaused = paused; }
	bool IsPaused() const { return m_Paused; }

private:
	struct PresentEntry
	{
		constexpr static uint64 sQPCDroppedFrame = ~0ull;		///< QPC when frame is dropped (due to tearing)
		constexpr static uint64 sQPCMissedFrame	 = ~0ull - 1;	///< QPC when frame was missed on CPU (rare)

		uint64 PresentQPC = sQPCDroppedFrame;
		uint64 DisplayQPC = sQPCDroppedFrame;
		uint32 PresentID  = ~0u;
		uint32 FrameIndex = 0;
	};

	int& GetCurrentThreadTrackIndex()
	{
		static thread_local int index = -1;
		return index;
	}

	EventTrack& GetCurrentThreadTrack()
	{
		int index = GetCurrentThreadTrackIndex();
		if (index == -1)
			index = RegisterCurrentThread();
		return m_Tracks[index];
	}

	PresentEntry* GetPresentEntry(uint32 presentID, bool isNewEntry)
	{
		PresentEntry& entry = m_PresentQueue[presentID % m_PresentQueue.size()];
		if (entry.PresentID != presentID && !isNewEntry)
			return nullptr;
		return &entry;
	}

	IDXGISwapChain*				  m_pPresentSwapChain			= nullptr; ///< The swapchain that was last presented with
	int							  m_PresentTrackIndex			= -1;	   ///< The track index for the present timeline
	StaticArray<PresentEntry, 32> m_PresentQueue				= {};	   ///< Queue to register information whenever Present is called
	uint32						  m_LastQueuedPresentID			= 0;	   ///< PresentID after the last Present was called
	uint32						  m_LastQueriedPresentID		= 0;	   ///< The last PresentID which GetFrameStatistics has provided data for
	uint32						  m_LastProcessedValidPresentID = 0;	   ///< The last PresentID which GetFrameStatistics has provided data for
	uint32						  m_LastSyncRefreshCount		= 0;	   ///< The SyncRefreshCount of the last queried frame statistics
	uint32						  m_LastProcessedPresentID		= 0;	   ///< The last PresentID which was processed to an even
	uint32						  m_MsToTicks					= 0;	   ///< The amount of ticks in 1 ms

	CPUProfilerCallbacks		 m_EventCallback;
	Mutex						 m_ThreadDataLock;			///< Mutex for accessing thread data
	Array<EventTrack>			 m_Tracks;
	Array<uint64>				 m_BeginFrameTicks;
	uint32						 m_HistorySize	 = 0;		///< History size
	uint32						 m_FrameIndex	 = 0;		///< The current frame index
	bool						 m_Paused		 = false;	///< The current pause state
	bool						 m_QueuedPaused	 = false;	///< The queued pause state
	bool						 m_IsInitialized = false;
};

// Helper RAII-style structure to push and pop a CPU sample region
struct CPUProfileScope
{
	CPUProfileScope(const char* pFunctionName, const char* pFilePath, uint32 lineNumber, const char* pName, uint32 color = 0)
	{
		gProfiler.BeginEvent(pName, color, pFilePath, lineNumber);
	}

	CPUProfileScope(const char* pFunctionName, const char* pFilePath, uint32 lineNumber, uint32 color = 0)
	{
		gProfiler.BeginEvent(pFunctionName, color, pFilePath, lineNumber);
	}

	~CPUProfileScope()
	{
		gProfiler.EndEvent();
	}

	CPUProfileScope(const CPUProfileScope&)			   = delete;
	CPUProfileScope& operator=(const CPUProfileScope&) = delete;
};

}

#endif