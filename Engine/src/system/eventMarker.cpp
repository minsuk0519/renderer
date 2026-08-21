#include <system/eventMarker.hpp>
#include <system/eventProf.hpp>

#include <cstring>

#if ENGINE_DEBUG_GPUEVENT

namespace prof
{
	ScopedGPUEvent::ScopedGPUEvent(ID3D12GraphicsCommandList* cmdList, EVENT_INDEX nameID)
		: cmdList(cmdList)
	{
		const char* name = prof::getEventName(nameID);
		cmdList->BeginEvent(1, name, (UINT)strlen(name) + 1);
#if ENGINE_DEBUG_GPUPROF
		profEventIndex = prof::beginGPUProfEvent(cmdList, nameID);
#endif // ENGINE_DEBUG_GPUPROF
	}

	ScopedGPUEvent::~ScopedGPUEvent()
	{
#if ENGINE_DEBUG_GPUPROF
		if (profEventIndex >= 0)
		{
			prof::endGPUProfEvent(cmdList, profEventIndex);
		}
#endif // ENGINE_DEBUG_GPUPROF
		cmdList->EndEvent();
	}
}

#endif // ENGINE_DEBUG_GPUEVENT

#if ENGINE_DEBUG_CPUEVENT

namespace prof
{
	ScopedCPUEvent::ScopedCPUEvent(EVENT_INDEX nameID)
	{
#if ENGINE_DEBUG_CPUPROF
		profEventIndex = prof::beginCPUProfEvent(nameID);
#endif // ENGINE_DEBUG_CPUPROF
	}

	ScopedCPUEvent::~ScopedCPUEvent()
	{
#if ENGINE_DEBUG_CPUPROF
		if (profEventIndex >= 0)
		{
			prof::endCPUProfEvent(profEventIndex);
		}
#endif // ENGINE_DEBUG_CPUPROF
	}
}

#endif // ENGINE_DEBUG_CPUEVENT
