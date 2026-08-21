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

			// Delegate backend initialization to render_prof
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

			QueryPerformanceFrequency(&qpcFrequency);

			for (uint i = 0; i < CPUPROF_MAX_THREADS; ++i)
			{
				laneThreadId[i] = 0;
				laneReset(lanes[i]);
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

			// Claim a lane for this thread on first use
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

			// Record begin event
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

			// Compute elapsed time
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

		void cpuProfiler::endFrame()
		{
			// Main thread aggregates all lanes and computes frame totals
			for (uint i = 0; i < nextLane.load(); ++i)
			{
				profLane& lane = lanes[i];
				lane.totalMs = 0.0;

				// Accumulate depth-0 events for lane total
				for (const auto& event : lane.pendingEvents)
				{
					if (event.depth == 0)
					{
						lane.totalMs += event.timeMs;
					}
				}

				// Move pending to lastFrame
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

				// Compute and log thread totals
				for (uint i = 0; i < nextLane.load(); ++i)
				{
					TC_LOG(std::format("[CPUProf] Thread {} Total: {:.3f} ms", i, lanes[i].totalMs).c_str());
				}

				TC_LOG("=== End CPU Profiler Frame 60 ===");
				frameIndex = 0;
			}
#endif

			// Reset all claimed lanes
			for (uint i = 0; i < nextLane.load(); ++i)
			{
				laneReset(lanes[i]);
			}

			frameActive = true;
			++frameIndex;
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

#endif  // ENGINE_DEBUG_CPUPROF

}  // namespace prof

#endif  // ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF
