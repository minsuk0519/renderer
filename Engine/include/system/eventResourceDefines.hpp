#pragma once

#include <system/defines.hpp>
#include <system/eventsDefines.hpp>
#include <cstdint>

#if ENGINE_DEBUG_EVENTRESOURCE

namespace prof
{
	constexpr uint EVENTRESOURCE_MAX_LOC = 256;
	constexpr uint EVENTRESOURCE_L2_MAX_ENTRIES = 256;
	constexpr uint EVENTRESOURCE_MAX_PER_EVENT = 32;

	struct eventResourceEntry
	{
		uint bufferId;
		uint16_t hlslLoc;
		uint16_t heapSlot;
	};

	void flushEventResourceScope(void* cmdList, EVENT_INDEX nameID);

	void accumulateEventResource(EVENT_INDEX nameID, uint bufferId, uint hlslLoc, uint heapSlot);

	// Flipping OFF->ON clears the store, so each capture session starts fresh.
	void setEventResourceCaptureActive(bool active);
	bool isEventResourceCaptureActive();

	uint getEventResourceCount(EVENT_INDEX id);
	const eventResourceEntry* getEventResource(EVENT_INDEX id, uint i);

}  // namespace prof

#endif // ENGINE_DEBUG_EVENTRESOURCE
