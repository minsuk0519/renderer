#pragma once

#include <system/defines.hpp>
#include <render/gpuEventsDefines.hpp>

#include <d3d12.h>

namespace render
{
#if ENGINE_DEBUG_GPUEVENT
	class ScopedGPUEvent
	{
	public:
		ScopedGPUEvent(ID3D12GraphicsCommandList* cmdList, GPUEVENT_INDEX nameID);
		~ScopedGPUEvent();

		ScopedGPUEvent(const ScopedGPUEvent&) = delete;
		ScopedGPUEvent& operator=(const ScopedGPUEvent&) = delete;

	private:
		ID3D12GraphicsCommandList* cmdList;
		int profEvent = -1;
	};

	// never to derive the event ID (the ID comes from the string via the variable template).
	#define GPU_EVENT_CONCAT_(a, b) a##b
	#define GPU_EVENT_CONCAT(a, b) GPU_EVENT_CONCAT_(a, b)
	#define GPU_EVENT(cmdList, name) \
		render::ScopedGPUEvent GPU_EVENT_CONCAT(gpuScopedEvent_, __COUNTER__)((cmdList), render::gpuEventID<name>)
#else
	#define GPU_EVENT(cmdList, name) ((void)(cmdList))
#endif
};
