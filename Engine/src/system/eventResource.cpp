#include <system/eventResourceDefines.hpp>
#include <system/logger.hpp>

#if ENGINE_DEBUG_EVENTRESOURCE
#include <render/render_eventResource.hpp>
#endif // ENGINE_DEBUG_EVENTRESOURCE

#if ENGINE_DEBUG_EVENTRESOURCE

namespace prof
{
	static eventResourceEntry store[EVENT_CAPACITY][EVENTRESOURCE_MAX_PER_EVENT];
	static uint storeCount[EVENT_CAPACITY] = {};

	static bool captureActive = false;
	static bool storeFullWarned = false;

	void accumulateEventResource(EVENT_INDEX nameID, uint bufferId, uint hlslLoc, uint heapSlot)
	{
		if (nameID < 0 || nameID >= EVENT_CAPACITY)
		{
			return;
		}

		uint& count = storeCount[nameID];
		if (count >= EVENTRESOURCE_MAX_PER_EVENT)
		{
			if (!storeFullWarned)
			{
				TC_LOG_WARNING("Event resource store full for event");
				storeFullWarned = true;
			}
			return;
		}

		for (uint i = 0; i < count; ++i)
		{
			eventResourceEntry& entry = store[nameID][i];
			if (entry.hlslLoc == hlslLoc && entry.bufferId == bufferId)
			{
				return;
			}
		}

		store[nameID][count] = {bufferId, (uint16_t)hlslLoc, (uint16_t)heapSlot};
		++count;
	}

	void setEventResourceCaptureActive(bool active)
	{
		if (active && !captureActive)
		{
			for (int i = 0; i < EVENT_CAPACITY; ++i)
			{
				storeCount[i] = 0;
			}
			storeFullWarned = false;
		}
		captureActive = active;
	}

	bool isEventResourceCaptureActive()
	{
		return captureActive;
	}

	uint getEventResourceCount(EVENT_INDEX id)
	{
		if (id < 0 || id >= EVENT_CAPACITY)
		{
			return 0;
		}
		return storeCount[id];
	}

	const eventResourceEntry* getEventResource(EVENT_INDEX id, uint i)
	{
		if (id < 0 || id >= EVENT_CAPACITY)
		{
			return nullptr;
		}
		if (i >= storeCount[id])
		{
			return nullptr;
		}
		return &store[id][i];
	}

	void flushEventResourceScope(void* cmdList, EVENT_INDEX nameID)
	{
		render::flushEventResourceScopeBackend(cmdList, nameID);
	}

}  // namespace prof

#endif // ENGINE_DEBUG_EVENTRESOURCE
