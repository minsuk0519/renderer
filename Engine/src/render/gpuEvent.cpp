#include <render/gpuEvent.hpp>
#include <render/GPUProf.hpp>

#include <cstring>

namespace render
{
	ScopedGPUEvent::ScopedGPUEvent(ID3D12GraphicsCommandList* cmdList, GPUEVENT_INDEX nameID)
		: cmdList(cmdList)
	{
		const char* name = render::getGPUEventName(nameID);
		cmdList->BeginEvent(1, name, (UINT)strlen(name) + 1);
#if ENGINE_DEBUG_GPUPROF
		profEvent = render::beginGPUProfEvent(cmdList, nameID);
#endif // ENGINE_DEBUG_GPUPROF
	}

	ScopedGPUEvent::~ScopedGPUEvent()
	{
#if ENGINE_DEBUG_GPUPROF
		if (profEvent >= 0)
		{
			render::endGPUProfEvent(cmdList, profEvent);
		}
#endif // ENGINE_DEBUG_GPUPROF
		cmdList->EndEvent();
	}
}
