#include <system/eventProf.hpp>

#if ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF

#include <system/eventProfLane.hpp>
#include <system/logger.hpp>
#include <atomic>
#include <thread>
#include <windows.h>

#if ENGINE_DEBUG_GPUPROF
#include <render/render_prof.hpp>
#endif

namespace prof
{

#if ENGINE_DEBUG_GPUPROF

	namespace gpuProf
	{
		class gpuProfiler
		{
		public:
			bool init();
			void close();
			void endFrame();

		private:
			bool frameActive;
			bool enabled;
			UINT64 frameIndex;
			bool warnedMismatchedStack;
		};

		gpuProfiler g_gpuProf;

		bool gpuProfiler::init()
		{
			frameActive = true;
			enabled = true;
			frameIndex = 0;
			warnedMismatchedStack = false;

#if ENGINE_DEBUG_GPUPROF_LOG
			TC_LOG("GPU Profiler initialized");
			TC_LOG("=== Registered GPU Events ===");
			for (int i = 0; i < prof::getEventCount(); ++i)
			{
				TC_LOG(std::format("[{}] {}", i, prof::getEventName(static_cast<prof::EVENT_INDEX>(i))).c_str());
			}
#endif

			return render::initGPUProfBackend();
		}

		void gpuProfiler::close()
		{
			render::closeGPUProfBackend();
		}

		void gpuProfiler::endFrame()
		{
			render::endGPUProfFrameBackend();

			frameActive = true;
			++frameIndex;
		}
	}

	bool initGPUProf()
	{
		return gpuProf::g_gpuProf.init();
	}

	void closeGPUProf()
	{
		gpuProf::g_gpuProf.close();
	}

	void endGPUProfFrame()
	{
		gpuProf::g_gpuProf.endFrame();
	}

	int beginGPUProfEvent(void* cmdList, EVENT_INDEX nameID)
	{
		if (!cmdList)
			return -1;

		return render::beginGPUProfEventBackend(cmdList, nameID);
	}

	void endGPUProfEvent(void* cmdList, int eventIndex)
	{
		if (!cmdList)
			return;

		render::endGPUProfEventBackend(cmdList, eventIndex);
	}

	uint getGPULaneCount()
	{
		return render::getGPULaneCountBackend();
	}

	const profLaneView* getGPULaneView(uint lane)
	{
		return render::getGPULaneViewBackend(lane);
	}

	const float* getGPUEventHistory(EVENT_INDEX eventID)
	{
		return render::getGPUEventHistoryBackend(eventID);
	}

	uint getGPUHistoryOffset()
	{
		return render::getGPUHistoryOffsetBackend();
	}

	uint getGPUEventCatalogCount()
	{
		return render::getGPUEventCatalogCountBackend();
	}

	const profEventInfoView* getGPUEventCatalog(EVENT_INDEX id)
	{
		return render::getGPUEventCatalogBackend(id);
	}

#endif  // ENGINE_DEBUG_GPUPROF

#if ENGINE_DEBUG_CPUPROF

	namespace cpuProf
	{
		constexpr uint CPUPROF_MAX_THREADS = 8;

		class cpuProfiler
		{
		public:
			bool init();
			void close();
			void endFrame();
			int beginEvent(EVENT_INDEX nameID);
			void endEvent(int eventIndex);

			uint                 laneCount() const;
			const profLaneView*  laneView(uint lane) const;
			const float*         eventHistoryFor(EVENT_INDEX id) const;
			uint                 historyOffset() const;
			float                frameTotal() const;
			const float*         frameTotalHistoryData() const;

		private:
			profLane lanes[CPUPROF_MAX_THREADS];
			DWORD laneThreadId[CPUPROF_MAX_THREADS];
			std::atomic<uint> nextLane;
			LARGE_INTEGER qpcFrequency;
			bool frameActive;
			bool enabled;
			UINT64 frameIndex;
			bool warnedMismatchedStack;
			bool warnedNegativeTime;
			bool warnedThreadOverflow;

			profEventView snapshotEvents[CPUPROF_MAX_THREADS][PROF_MAX_EVENTS_PER_LANE];
			profLaneView snapshotLanes[CPUPROF_MAX_THREADS];
			char laneLabels[CPUPROF_MAX_THREADS][16];
			float laneTotalHistory[CPUPROF_MAX_THREADS][PROF_HISTORY_FRAMES];
			float eventHistory[EVENT_CAPACITY][PROF_HISTORY_FRAMES];
			float frameTotalMs;
			float frameTotalHistory[PROF_HISTORY_FRAMES];
			uint historyHead;

			void publishSnapshot();
		};

		cpuProfiler g_cpuProf;
		thread_local int t_laneIndex = -1;

		bool cpuProfiler::init()
		{
			frameActive = true;
			enabled = true;
			frameIndex = 0;
			warnedMismatchedStack = false;
			warnedNegativeTime = false;
			warnedThreadOverflow = false;
			nextLane = 0;
			historyHead = 0;

			QueryPerformanceFrequency(&qpcFrequency);

			for (uint i = 0; i < CPUPROF_MAX_THREADS; ++i)
			{
				laneThreadId[i] = 0;
				laneReset(lanes[i]);
				snprintf(laneLabels[i], sizeof(laneLabels[i]), "Thread %u", i);
			}

			for (uint i = 0; i < CPUPROF_MAX_THREADS; ++i)
			{
				snapshotLanes[i] = {};
				for (uint j = 0; j < PROF_MAX_EVENTS_PER_LANE; ++j)
				{
					snapshotEvents[i][j] = {};
				}
				for (uint j = 0; j < PROF_HISTORY_FRAMES; ++j)
				{
					laneTotalHistory[i][j] = 0.0f;
				}
			}
			for (uint i = 0; i < EVENT_CAPACITY; ++i)
			{
				for (uint j = 0; j < PROF_HISTORY_FRAMES; ++j)
				{
					eventHistory[i][j] = 0.0f;
				}
			}

			frameTotalMs = 0.0f;
			for (uint j = 0; j < PROF_HISTORY_FRAMES; ++j)
			{
				frameTotalHistory[j] = 0.0f;
			}

#if ENGINE_DEBUG_CPUPROF_LOG
			TC_LOG("CPU Profiler initialized");
			TC_LOG("=== Registered CPU Events ===");
			for (int i = 0; i < prof::getEventCount(); ++i)
			{
				TC_LOG(std::format("[{}] {}", i, prof::getEventName(static_cast<prof::EVENT_INDEX>(i))).c_str());
			}
#endif

			return true;
		}

		void cpuProfiler::close()
		{
			for (uint i = 0; i < CPUPROF_MAX_THREADS; ++i)
			{
				laneReset(lanes[i]);
			}
		}

		int cpuProfiler::beginEvent(EVENT_INDEX nameID)
		{
			if (!enabled || !frameActive)
				return -1;

			if (t_laneIndex == -1)
			{
				uint laneIdx = nextLane.fetch_add(1);
				if (laneIdx >= CPUPROF_MAX_THREADS)
				{
					if (!warnedThreadOverflow)
					{
						TC_LOG(std::format("CPU Profiler: thread overflow (max {} threads)", CPUPROF_MAX_THREADS).c_str());
						warnedThreadOverflow = true;
					}
					return -1;
				}
				t_laneIndex = laneIdx;
				laneThreadId[laneIdx] = GetCurrentThreadId();
			}

			profLane& lane = lanes[t_laneIndex];

			int eventIndex = laneBegin(lane, nameID, std::format("T{}", t_laneIndex).c_str());

			if (eventIndex >= 0)
			{
				// Capture timestamp AFTER lane bookkeeping, so measurement excludes overhead
				LARGE_INTEGER now;
				QueryPerformanceCounter(&now);
				lane.pendingEvents[eventIndex].beginTick = now.QuadPart;
			}

			return eventIndex;
		}

		void cpuProfiler::endEvent(int eventIndex)
		{
			if (!frameActive || t_laneIndex < 0 || t_laneIndex >= CPUPROF_MAX_THREADS)
				return;

			profLane& lane = lanes[t_laneIndex];

			// Capture timestamp FIRST, before lane bookkeeping
			LARGE_INTEGER now;
			QueryPerformanceCounter(&now);

			if (eventIndex < 0 || eventIndex >= static_cast<int>(lane.pendingEvents.size()))
				return;

			profEvent& event = lane.pendingEvents[eventIndex];

			UINT64 beginTick = event.beginTick;
			UINT64 endTick = now.QuadPart;

			if (endTick >= beginTick)
			{
				event.timeMs = (endTick - beginTick) * 1000.0 / qpcFrequency.QuadPart;
			}
			else
			{
				event.timeMs = 0.0;
				if (!warnedNegativeTime)
				{
					TC_LOG("CPU Profiler: negative elapsed time detected (QueryPerformanceCounter went backward); clamping to 0");
					warnedNegativeTime = true;
				}
			}

			laneEnd(lane, eventIndex, warnedMismatchedStack);
		}

		void cpuProfiler::publishSnapshot()
		{
			historyHead = (historyHead + 1) % PROF_HISTORY_FRAMES;

			for (uint i = 0; i < EVENT_CAPACITY; ++i)
			{
				eventHistory[i][historyHead] = 0.0f;
			}

			uint numClaimedLanes = nextLane.load();
			for (uint laneIdx = 0; laneIdx < numClaimedLanes && laneIdx < CPUPROF_MAX_THREADS; ++laneIdx)
			{
				profLane& lane = lanes[laneIdx];

				uint eventCount = (std::min)(static_cast<uint>(lane.lastFrameEvents.size()), PROF_MAX_EVENTS_PER_LANE);
				for (uint eventIdx = 0; eventIdx < eventCount; ++eventIdx)
				{
					const profEvent& event = lane.lastFrameEvents[eventIdx];
					snapshotEvents[laneIdx][eventIdx] = {
						prof::getEventName(event.nameID),
						event.depth,
						static_cast<float>(event.timeMs),
						event.nameID
					};

					if (event.nameID >= 0 && event.nameID < EVENT_CAPACITY)
					{
						eventHistory[event.nameID][historyHead] += static_cast<float>(event.timeMs);
					}
				}

				snapshotLanes[laneIdx].label = laneLabels[laneIdx];
				snapshotLanes[laneIdx].totalMs = static_cast<float>(lane.totalMs);
				snapshotLanes[laneIdx].events = snapshotEvents[laneIdx];
				snapshotLanes[laneIdx].eventCount = eventCount;
				snapshotLanes[laneIdx].totalHistory = laneTotalHistory[laneIdx];

				laneTotalHistory[laneIdx][historyHead] = static_cast<float>(lane.totalMs);
			}

			frameTotalMs = 0.0f;
			for (uint laneIdx = 0; laneIdx < numClaimedLanes && laneIdx < CPUPROF_MAX_THREADS; ++laneIdx)
			{
				frameTotalMs += snapshotLanes[laneIdx].totalMs;
			}
			frameTotalHistory[historyHead] = frameTotalMs;
		}

		void cpuProfiler::endFrame()
		{
			// Main thread aggregates all lanes and computes frame totals
			for (uint i = 0; i < nextLane.load(); ++i)
			{
				profLane& lane = lanes[i];
				lane.totalMs = 0.0;

				for (const auto& event : lane.pendingEvents)
				{
					if (event.depth == 0)
					{
						lane.totalMs += event.timeMs;
					}
				}

				lane.lastFrameEvents = lane.pendingEvents;
			}

#if ENGINE_DEBUG_CPUPROF_LOG
			if (frameIndex == 60)
			{
				TC_LOG("=== CPU Profiler Frame 60 ===");
				for (uint i = 0; i < nextLane.load(); ++i)
				{
					profLane& lane = lanes[i];
					std::string laneLabel = std::format("T{}", i);
					laneDump(lane, laneLabel.c_str());
				}

				for (uint i = 0; i < nextLane.load(); ++i)
				{
					TC_LOG(std::format("[CPUProf] Thread {} Total: {:.3f} ms", i, lanes[i].totalMs).c_str());
				}

				TC_LOG("=== End CPU Profiler Frame 60 ===");
				frameIndex = 0;
			}
#endif

			publishSnapshot();

			for (uint i = 0; i < nextLane.load(); ++i)
			{
				laneReset(lanes[i]);
			}

			frameActive = true;
			++frameIndex;
		}

		uint cpuProfiler::laneCount() const
		{
			return nextLane.load();
		}

		const profLaneView* cpuProfiler::laneView(uint lane) const
		{
			if (lane >= CPUPROF_MAX_THREADS)
				return nullptr;
			return &snapshotLanes[lane];
		}

		const float* cpuProfiler::eventHistoryFor(EVENT_INDEX id) const
		{
			if (id < 0 || id >= EVENT_CAPACITY)
				return nullptr;
			return eventHistory[id];
		}

		uint cpuProfiler::historyOffset() const
		{
			return (historyHead + 1) % PROF_HISTORY_FRAMES;
		}

		float cpuProfiler::frameTotal() const
		{
			return frameTotalMs;
		}

		const float* cpuProfiler::frameTotalHistoryData() const
		{
			return frameTotalHistory;
		}
	}

	bool initCPUProf()
	{
		return cpuProf::g_cpuProf.init();
	}

	void closeCPUProf()
	{
		cpuProf::g_cpuProf.close();
	}

	void endCPUProfFrame()
	{
		cpuProf::g_cpuProf.endFrame();
	}

	int beginCPUProfEvent(EVENT_INDEX nameID)
	{
		return cpuProf::g_cpuProf.beginEvent(nameID);
	}

	void endCPUProfEvent(int eventIndex)
	{
		cpuProf::g_cpuProf.endEvent(eventIndex);
	}

	uint getCPULaneCount()
	{
		return cpuProf::g_cpuProf.laneCount();
	}

	const profLaneView* getCPULaneView(uint lane)
	{
		return cpuProf::g_cpuProf.laneView(lane);
	}

	const float* getCPUEventHistory(EVENT_INDEX eventID)
	{
		return cpuProf::g_cpuProf.eventHistoryFor(eventID);
	}

	uint getCPUHistoryOffset()
	{
		return cpuProf::g_cpuProf.historyOffset();
	}

	float getCPUFrameTotalMs()
	{
		return cpuProf::g_cpuProf.frameTotal();
	}

	const float* getCPUFrameTotalHistory()
	{
		return cpuProf::g_cpuProf.frameTotalHistoryData();
	}

#endif  // ENGINE_DEBUG_CPUPROF

}  // namespace prof

#endif  // ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF
