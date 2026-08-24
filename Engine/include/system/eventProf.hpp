#pragma once

#include <system/defines.hpp>
#include <system/eventsDefines.hpp>

#if ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF

namespace prof
{
	constexpr uint PROF_HISTORY_FRAMES = 120;   // matches renderer::CULLSTATS_HISTORY

	struct profEventView          // flat POD; no std::vector crosses the boundary
	{
		const char* name;
		uint  depth;
		float timeMs;
		EVENT_INDEX nameID;
	};

	struct profLaneView
	{
		const char* label;
		float totalMs;
		const profEventView* events;
		uint  eventCount;
		const float* totalHistory;
	};

	struct profEventInfoView
	{
		EVENT_INDEX nameID;
		uint  lane;
		uint  depth;
		bool  activeThisFrame;
	};
}

#endif // ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF

#if ENGINE_DEBUG_GPUPROF

namespace prof
{
	bool initGPUProf();
	void closeGPUProf();
	void endGPUProfFrame();
	int  beginGPUProfEvent(void* cmdList, EVENT_INDEX nameID);  // void* to avoid including d3d12.h
	void endGPUProfEvent(void* cmdList, int eventIndex);

	uint getGPULaneCount();
	const profLaneView* getGPULaneView(uint lane);
	const float* getGPUEventHistory(EVENT_INDEX);
	uint getGPUHistoryOffset();

	uint getGPUEventCatalogCount();
	const profEventInfoView* getGPUEventCatalog(EVENT_INDEX id);
}  // namespace prof

#endif // ENGINE_DEBUG_GPUPROF

#if ENGINE_DEBUG_CPUPROF

namespace prof
{
	bool initCPUProf();
	void closeCPUProf();
	void endCPUProfFrame();
	int  beginCPUProfEvent(EVENT_INDEX nameID);
	void endCPUProfEvent(int eventIndex);

	uint getCPULaneCount();
	const profLaneView* getCPULaneView(uint lane);
	const float* getCPUEventHistory(EVENT_INDEX);
	uint getCPUHistoryOffset();
	float getCPUFrameTotalMs();
	const float* getCPUFrameTotalHistory();
}  // namespace prof

#endif // ENGINE_DEBUG_CPUPROF
