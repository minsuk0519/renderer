#pragma once

#include <system/defines.hpp>
#include <system/eventsDefines.hpp>
#include <system/logger.hpp>
#include <vector>
#include <format>
#include <windows.h>

#if ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF

namespace prof
{
	constexpr uint PROF_MAX_EVENTS_PER_LANE = 256;

	struct profEvent
	{
		EVENT_INDEX nameID;
		uint depth;
		int parent;
		uint slot;
		UINT64 beginTick;
		double timeMs;
	};

	struct profLane
	{
		std::vector<int> openStack;
		std::vector<profEvent> pendingEvents;
		std::vector<profEvent> lastFrameEvents;
		uint recordedCount;
		uint resolvedCount;
		double totalMs;
		bool warnedFull;
	};

	inline int laneBegin(profLane& lane, EVENT_INDEX nameID, const char* laneLabel)
	{
		if (lane.recordedCount >= PROF_MAX_EVENTS_PER_LANE)
		{
			if (!lane.warnedFull)
			{
				TC_LOG(std::format("Event Profiler: {} lane event buffer full (max {} events)",
					laneLabel, PROF_MAX_EVENTS_PER_LANE).c_str());
				lane.warnedFull = true;
			}
			return -1;
		}

		profEvent event = {};
		event.nameID = nameID;
		event.depth = static_cast<uint>(lane.openStack.size());
		event.parent = lane.openStack.empty() ? -1 : static_cast<int>(lane.openStack.back());
		event.slot = lane.recordedCount * 2;
		event.beginTick = 0;
		event.timeMs = 0.0;

		int eventIndex = static_cast<int>(lane.pendingEvents.size());
		lane.pendingEvents.push_back(event);

		lane.openStack.push_back(eventIndex);
		++lane.recordedCount;

		return eventIndex;
	}

	inline void laneEnd(profLane& lane, int eventIndex, bool& warnedMismatch)
	{
		if (eventIndex < 0 || eventIndex >= static_cast<int>(lane.pendingEvents.size()))
			return;

		if (!lane.openStack.empty() && lane.openStack.back() == eventIndex)
		{
			lane.openStack.pop_back();
		}
		else if (!lane.openStack.empty() && !warnedMismatch)
		{
			TC_LOG("Event Profiler: event stack mismatch (scope nesting error)");
			warnedMismatch = true;
		}
	}

	inline void laneReset(profLane& lane)
	{
		lane.openStack.clear();
		lane.pendingEvents.clear();
		lane.lastFrameEvents.clear();
		lane.recordedCount = 0;
		lane.resolvedCount = 0;
		lane.totalMs = 0.0;
		lane.warnedFull = false;
	}

	inline void laneDump(const profLane& lane, const char* laneLabel)
	{
		for (const auto& event : lane.lastFrameEvents)
		{
			std::string indent;
			for (uint d = 0; d < event.depth; ++d)
				indent += "  ";

			TC_LOG(std::format("[EventProf] {}{}  lane={} {:.3f} ms",
				indent, prof::getEventName(event.nameID), laneLabel, event.timeMs).c_str());
		}
	}

}  // namespace prof

#endif  // ENGINE_DEBUG_GPUPROF || ENGINE_DEBUG_CPUPROF
