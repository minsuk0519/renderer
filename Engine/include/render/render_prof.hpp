#pragma once

#include <system/defines.hpp>
#include <system/eventsDefines.hpp>

#if ENGINE_DEBUG_GPUPROF

namespace prof { struct profLaneView; }

namespace render
{
	bool initGPUProfBackend();
	void closeGPUProfBackend();
	void endGPUProfFrameBackend();
	int  beginGPUProfEventBackend(void* cmdList, prof::EVENT_INDEX nameID);
	void endGPUProfEventBackend(void* cmdList, int eventIndex);

	uint getGPULaneCountBackend();
	const prof::profLaneView* getGPULaneViewBackend(uint lane);
	const float* getGPUEventHistoryBackend(prof::EVENT_INDEX);
	uint getGPUHistoryOffsetBackend();

}  // namespace render

#endif // ENGINE_DEBUG_GPUPROF
