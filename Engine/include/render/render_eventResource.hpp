#pragma once

#include <system/defines.hpp>
#include <system/eventsDefines.hpp>
#include <cstdint>

#if ENGINE_DEBUG_EVENTRESOURCE

namespace render
{
	constexpr uint EVENTRESOURCE_MAX_LOC = 256;
	constexpr uint EVENTRESOURCE_MAX_PER_EVENT = 32;

	struct bindScratchEntry
	{
		uint bufferId;
		uint16_t heapSlot;
	};

	struct bindScratchTable
	{
		bindScratchEntry locs[EVENTRESOURCE_MAX_LOC];
	};

	struct indirectBindScratchTable
	{
		uint16_t indices[EVENTRESOURCE_MAX_LOC];
		uint count = 0;
		bool warnedFull = false;

		void clear()
		{
			count = 0;
			warnedFull = false;
		}
	};

	void flushEventResourceScopeBackend(void* cmdList, prof::EVENT_INDEX nameID);
	const char* getEventResourceNameBackend(uint bufferId);

}  // namespace render

#endif // ENGINE_DEBUG_EVENTRESOURCE
