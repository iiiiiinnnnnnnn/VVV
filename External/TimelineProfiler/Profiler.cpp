// Modified for VEER: CPU-only TimelineProfiler sample, isolated in TimelineProfiler namespace.

#include "Profiler.h"

#if WITH_PROFILING

namespace TimelineProfiler
{

Profiler gProfiler;


// Thread-safe page allocator that recycles pages based on ID
struct ProfilerAllocator
{
public:
	static constexpr uint32 cPageSize = 2 * 1024;

	struct Page
	{
		uint32 ID	= 0;
		uint32 Size = 0;

		void* GetData()
		{
			return static_cast<void*>(this + 1);
		}

		static Page* Create(uint32 size)
		{
			void* pData = new char[sizeof(Page) + size];
			Page* pPage = new (pData) Page;
			pPage->Size = size;
			return pPage;
		}

		static void Release(Page* pPage)
		{
			char* pData = reinterpret_cast<char*>(pPage);
			delete[] pData;
		}
	};
	static_assert(std::is_trivially_destructible_v<Page>);

	void Release()
	{
		while (!AllocatedPages.empty())
		{
			Page* pPage = AllocatedPages.front();
			Page::Release(pPage);
			AllocatedPages.pop();
		}
		while (FreePages.empty())
		{
			Page* pPage = FreePages.back();
			Page::Release(pPage);
			FreePages.pop_back();
		}
	}

	Page* AllocatePage(uint32 id)
	{
		std::lock_guard m(PageLock);

		Page* pPage = nullptr;
		if (FreePages.empty())
		{
			pPage = Page::Create(cPageSize);
			++NumPages;
		}
		else
		{
			pPage = FreePages.back();
			FreePages.pop_back();
		}
		pPage->ID = id;
		AllocatedPages.push(pPage);
		return pPage;
	}

	bool IsValidPage(uint32 id)
	{
		return id >= MinValidID;
	}

	void Evict(uint32 id)
	{
		std::lock_guard m(PageLock);

		gAssert(NumPages == FreePages.size() + AllocatedPages.size());

		while (!AllocatedPages.empty())
		{
			Page* pPage = AllocatedPages.front();
			if (id >= pPage->ID)
			{
				FreePages.push_back(pPage);
				AllocatedPages.pop();
			}
			else
			{
				break;
			}
		}

		MinValidID = id + 1;
	}

private:
	std::mutex		  PageLock;
	Array<Page*>	  FreePages;
	std::queue<Page*> AllocatedPages;
	uint32			  MinValidID = 0;
	int				  NumPages	 = 0;
};

static ProfilerAllocator sProfilerAllocator;

class SubAllocator
{
public:
	const char* String(const char* pStr, uint32 id)
	{
		uint32 len	 = (uint32)strlen(pStr) + 1;
		char*  pData = (char*)Allocate(len, id);
		strcpy_s(pData, len, pStr);
		return pData;
	}

	void* Allocate(uint32 size, uint32 id)
	{
		if (pPage == nullptr || id > ID || !sProfilerAllocator.IsValidPage(ID) || Offset + size > pPage->Size)
		{
			ID	   = std::max(ID, id);
			pPage  = sProfilerAllocator.AllocatePage(ID);
			Offset = 0;
		}
		void* pData = static_cast<char*>(pPage->GetData()) + Offset;
		Offset += size;
		return pData;
	}

private:
	uint32						 Offset = 0;
	uint32						 ID		= 0;
	ProfilerAllocator::Page*	 pPage	= nullptr;
};
static thread_local SubAllocator gAllocator;

static uint32 HSV2RGB(float hue, float saturation, float value)
{
	float R = std::max(std::min(std::fabs(hue * 6 - 3) - 1, 1.0f), 0.0f);
	float G = std::max(std::min(2 - std::fabs(hue * 6 - 2), 1.0f), 0.0f);
	float B = std::max(std::min(2 - std::fabs(hue * 6 - 4), 1.0f), 0.0f);

	R = ((R - 1) * saturation + 1) * value;
	G = ((G - 1) * saturation + 1) * value;
	B = ((B - 1) * saturation + 1) * value;

	return ((uint8)roundf(R * 255.0f) << 0) |
		   ((uint8)roundf(G * 255.0f) << 8) |
		   ((uint8)roundf(B * 255.0f) << 16) |
		   255 << 24;
}

static uint32 GetFrameColor(uint32 frameIndex)
{
	return HSV2RGB(frameIndex % 10 / 10.0f, 0.5f, 0.5f);
}

static uint32 ColorFromString(const char* pStr)
{
	const float saturation = 0.5f;
	const float value	   = 0.6f;
	float		hue		   = (float)std::hash<std::string>{}(pStr) / std::numeric_limits<size_t>::max();
	return HSV2RGB(hue, saturation, value);
}


void Profiler::Initialize(uint32 historySize)
{
	m_HistorySize	= historySize;
	m_IsInitialized = true;
	m_BeginFrameTicks.resize(historySize);

	uint64 frequency = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
	m_MsToTicks = (uint32)(frequency / 1000);
}

void Profiler::Shutdown()
{
	m_Tracks.clear();
	m_BeginFrameTicks.clear();

	sProfilerAllocator.Release();
}

// Begin a new CPU event on the current thread
void Profiler::BeginEvent(const char* pName, uint32 color, const char* pFilePath, uint32 lineNumber)
{
	if (!m_IsInitialized)
		return;

	if (m_EventCallback.OnEventBegin)
		m_EventCallback.OnEventBegin(pName, m_EventCallback.pUserData);

	if (m_Paused)
		return;

	// Record new event
	EventTrack&		   track	 = GetCurrentThreadTrack();
	ProfilerEventData& eventData = track.GetFrameData(m_FrameIndex);
	track.EventStack.Push() = (uint32)eventData.size();

	ProfilerEvent& newEvent = eventData.emplace_back();
	newEvent.Depth			= track.EventStack.GetSize() - 1;
	newEvent.ThreadIndex	= track.Index;
	newEvent.pName			= gAllocator.String(pName, m_FrameIndex);
	newEvent.pFilePath		= pFilePath;
	newEvent.LineNumber		= lineNumber;
	newEvent.Color			= color == 0 ? ColorFromString(pName) : color;
	QueryPerformanceCounter((LARGE_INTEGER*)(&newEvent.TicksBegin));
}

// End the last pushed event on the current thread
void Profiler::EndEvent()
{
	if (!m_IsInitialized)
		return;

	if (m_EventCallback.OnEventEnd)
		m_EventCallback.OnEventEnd(m_EventCallback.pUserData);

	if (m_Paused)
		return;

	// End and pop an event of the stack
	EventTrack& track = GetCurrentThreadTrack();

	gAssert(track.EventStack.GetSize() > 0, "Event mismatch. Called EndEvent more than BeginEvent");
	uint32		   eventIndex = track.EventStack.Pop();
	ProfilerEvent& event	  = track.GetFrameData(m_FrameIndex)[eventIndex];
	QueryPerformanceCounter((LARGE_INTEGER*)(&event.TicksEnd));
}


void Profiler::AddEvent(uint32 trackIndex, const ProfilerEvent& event, uint32 frameIndex)
{
	EventTrack&		   track  = m_Tracks[trackIndex];
	ProfilerEventData& events = track.GetFrameData(frameIndex);

	// Name must be copied
	ProfilerEvent newEvent = event;
	newEvent.pName		   = gAllocator.String(newEvent.pName, frameIndex);

	events.push_back(newEvent);
}


void Profiler::Present(IDXGISwapChain* pSwapChain)
{
	if (m_PresentTrackIndex == -1)
		m_PresentTrackIndex = RegisterTrack("Present", EventTrack::EType::Present, 0);

	if (!m_Paused)
	{
		// Add an entry for the current present
		uint32 presentID;
		if (SUCCEEDED(pSwapChain->GetLastPresentCount(&presentID)))
		{
			// If the last queried present is larger than the current present,
			// that means the swapchain may have been recreated and we need to reset.
			if (m_LastQueuedPresentID > presentID || m_pPresentSwapChain != pSwapChain)
			{
				m_LastQueriedPresentID		  = 0;
				m_LastProcessedPresentID	  = 0;
				m_LastProcessedValidPresentID = 0;
				m_PresentQueue				  = {};
				m_LastSyncRefreshCount		  = 0;
				m_pPresentSwapChain			  = pSwapChain;
			}

			PresentEntry* pEntry = GetPresentEntry(presentID, true);
			*pEntry				 = {};
			QueryPerformanceCounter((LARGE_INTEGER*)&pEntry->PresentQPC);
			pEntry->PresentID  = presentID;
			pEntry->FrameIndex = m_FrameIndex;

			m_LastQueuedPresentID = presentID;
		}

		DXGI_FRAME_STATISTICS frameStats{};
		while (SUCCEEDED(pSwapChain->GetFrameStatistics(&frameStats)) && frameStats.PresentCount > m_LastQueriedPresentID)
		{
			gAssert(frameStats.SyncQPCTime.QuadPart != 0);

			// Update the entry that was presented. All the ones before that were not queried are considered dropped.
			PresentEntry* pEntry = GetPresentEntry(frameStats.PresentCount, false);
			if (pEntry)
				pEntry->DisplayQPC = frameStats.SyncQPCTime.QuadPart;

			// It's possible for the CPU to miss a refresh in the time multiple presents have been processed (and not dropped).
			// In this case, there isn't enough information to resolve it. Try to estimate, otherwise ignore the frame.
			uint32 numRefreshes = frameStats.SyncRefreshCount - m_LastSyncRefreshCount;
			if (frameStats.SyncRefreshCount > 0 && numRefreshes > 1)
			{
				// If the previous entry was not processed, it means we missed it in GetFrameStatistics
				PresentEntry* pPreviousEntry = GetPresentEntry(frameStats.PresentCount - 1, false);
				if (pPreviousEntry && pPreviousEntry->DisplayQPC == PresentEntry::sQPCDroppedFrame)
				{
					// If there was a valid present before the previous one, average the time to at least get some estimate
					PresentEntry* pPreviousPreviousEntry = GetPresentEntry(frameStats.PresentCount - 2, false);
					if (pPreviousPreviousEntry && pEntry)
					{
						// printf("MISSED FRAME: %d - ID %d\n", pPreviousEntry->FrameIndex, pPreviousEntry->PresentID);

						// Taking a guess: the sync happened one sync after the one before it (most likely timing to have been missed)
						uint64 duration			 = pEntry->DisplayQPC - pPreviousPreviousEntry->DisplayQPC;
						uint64 estimatedSyncTime = pPreviousPreviousEntry->DisplayQPC + duration / numRefreshes;

						pPreviousEntry->DisplayQPC = estimatedSyncTime;
					}
					else
					{
						// If there's no way to estimate, just drop it
						pPreviousEntry->DisplayQPC = PresentEntry::sQPCMissedFrame;
					}
				}
			}

			m_LastSyncRefreshCount = frameStats.SyncRefreshCount;
			m_LastQueriedPresentID = frameStats.PresentCount;
		}
	}

	uint32 stackSize = 0;

	// Process all entries up until the last present
	for (uint32 presentID = m_LastProcessedPresentID + 1; presentID < m_LastQueriedPresentID; ++presentID)
	{
		const PresentEntry* pToProcessEntry = GetPresentEntry(presentID, false);

		// Search from the last processed valid entry where the next one is
		const PresentEntry* pNextValidPresentEntry = nullptr;
		for (uint32 nextValidPresent = m_LastProcessedValidPresentID; nextValidPresent <= m_LastQueriedPresentID; ++nextValidPresent)
		{
			const PresentEntry* pEntry = GetPresentEntry(nextValidPresent, false);
			if (pEntry && pEntry->DisplayQPC != PresentEntry::sQPCDroppedFrame)
			{
				pNextValidPresentEntry		  = pEntry;
				m_LastProcessedValidPresentID = pEntry->PresentID;
			}
		}

		// No valid present yet, try again next frame
		if (!pNextValidPresentEntry)
			break;

		if (pToProcessEntry)
		{
			if (pToProcessEntry->DisplayQPC == PresentEntry::sQPCMissedFrame)
			{
				// Ignored frame (rare, see above)
			}
			else if (pToProcessEntry->DisplayQPC == PresentEntry::sQPCDroppedFrame)
			{
				ProfilerEvent event{
					.pName		= "Discarded",
					.Color		= GetFrameColor(pToProcessEntry->FrameIndex),
					.Depth		= 1,
					.TicksBegin = pNextValidPresentEntry->DisplayQPC,
					.TicksEnd	= pNextValidPresentEntry->DisplayQPC + m_MsToTicks,
				};
				AddEvent(m_PresentTrackIndex, event, pToProcessEntry->FrameIndex);
			}
			else
			{
				// If the timing of the refresh is the same as the current frame, stack the events. 
				// Wait for the next frame with a different refresh time
				if (pNextValidPresentEntry->DisplayQPC == pToProcessEntry->DisplayQPC)
					break;

				ProfilerEvent event{
					.pName		= "Present",
					.Color		= GetFrameColor(pToProcessEntry->FrameIndex),
					.Depth		= stackSize,
					.TicksBegin = pToProcessEntry->DisplayQPC,
					.TicksEnd	= pNextValidPresentEntry->DisplayQPC,
				};
				++stackSize;
				AddEvent(m_PresentTrackIndex, event, pToProcessEntry->FrameIndex);
			}
		}

		m_LastProcessedPresentID = presentID;
	}
}



// Process the last frame and advance
void Profiler::Tick()
{
	if (!m_IsInitialized)
		return;

	const bool wasPaused = m_Paused;
	// 停止へ入る瞬間に現在フレームを閉じ、再開時は停止時間を持ち越さない。
	if (m_FrameIndex && !wasPaused) EndEvent();
	m_Paused = m_QueuedPaused;
	if (m_Paused) return;

	// Advance the frame and reset its data
	++m_FrameIndex;

	std::scoped_lock lock(m_ThreadDataLock);
	for (EventTrack& track : m_Tracks)
	{
		ProfilerEventData& events = track.GetFrameData(m_FrameIndex);
		events.clear();
	}

	QueryPerformanceCounter((LARGE_INTEGER*)&m_BeginFrameTicks[m_FrameIndex % m_BeginFrameTicks.size()]);

	// Begin a "CPU Frame" event
	BeginEvent("CPU Frame", GetFrameColor(m_FrameIndex));

	if (m_FrameIndex >= m_HistorySize)
		sProfilerAllocator.Evict(m_FrameIndex - m_HistorySize);
}



// Register a new thread
int Profiler::RegisterCurrentThread(const char* pName)
{
	int& threadIndex = GetCurrentThreadTrackIndex();

	const char* pLocalName = pName;
	char name[256]{};
	if (!pName)
	{
		// If the name is not provided, retrieve it using GetThreadDescription()
		PWSTR pDescription = nullptr;
		if (SUCCEEDED(::GetThreadDescription(GetCurrentThread(), &pDescription)))
			WideCharToMultiByte(CP_UTF8, 0, pDescription, (int)wcslen(pDescription), name, ARRAYSIZE(name), nullptr, nullptr);
		pLocalName = name;
	}

	if (threadIndex == -1)
	{
		threadIndex = RegisterTrack(pLocalName, EventTrack::EType::CPU, GetCurrentThreadId());
	}
	else
	{
		strcpy_s(m_Tracks[threadIndex].Name, pLocalName);
	}
	return threadIndex;
}


// Register a new track
int Profiler::RegisterTrack(const char* pName, EventTrack::EType type, uint32 id)
{
	std::scoped_lock lock(m_ThreadDataLock);

	EventTrack& data = m_Tracks.emplace_back();
	strcpy_s(data.Name, ARRAYSIZE(data.Name), pName);
	data.ID	   = id;
	data.Index = (uint32)m_Tracks.size() - 1;
	data.Type  = type;
	data.Events.resize(m_HistorySize);

	return data.Index;
}


}

#endif
