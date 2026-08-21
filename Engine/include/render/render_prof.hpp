#pragma once

#include <system/defines.hpp>
#include <system/eventsDefines.hpp>

#if ENGINE_DEBUG_GPUPROF

namespace render
{
	bool initGPUProfBackend();
	void closeGPUProfBackend();
	void endGPUProfFrameBackend();
	int  beginGPUProfEventBackend(void* cmdList, prof::EVENT_INDEX nameID);
	void endGPUProfEventBackend(void* cmdList, int eventIndex);

}  // namespace render

#endif // ENGINE_DEBUG_GPUPROF
