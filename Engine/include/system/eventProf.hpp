#pragma once

#include <system/defines.hpp>
#include <system/eventsDefines.hpp>

#if ENGINE_DEBUG_GPUPROF

// Forward declaration; actual GPU types are in render/render_prof.hpp
namespace render { struct gpuProfInterface; }

namespace prof
{
	bool initGPUProf();
	void closeGPUProf();
	void endGPUProfFrame();
	int  beginGPUProfEvent(void* cmdList, EVENT_INDEX nameID);  // void* to avoid including d3d12.h
	void endGPUProfEvent(void* cmdList, int eventIndex);

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

}  // namespace prof

#endif // ENGINE_DEBUG_CPUPROF
