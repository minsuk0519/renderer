#pragma once

#include <system/defines.hpp>
#include <system/eventsDefines.hpp>

#if ENGINE_DEBUG_GPUEVENT
#include <d3d12.h>
#endif // ENGINE_DEBUG_GPUEVENT

namespace prof
{
#if ENGINE_DEBUG_GPUEVENT
	class ScopedGPUEvent
	{
	public:
		ScopedGPUEvent(ID3D12GraphicsCommandList* cmdList, EVENT_INDEX nameID);
		~ScopedGPUEvent();

		ScopedGPUEvent(const ScopedGPUEvent&) = delete;
		ScopedGPUEvent& operator=(const ScopedGPUEvent&) = delete;

	private:
		ID3D12GraphicsCommandList* cmdList;
		int profEventIndex = -1;
#if ENGINE_DEBUG_EVENTRESOURCE
		EVENT_INDEX nameID;
#endif // ENGINE_DEBUG_EVENTRESOURCE
	};

	#define GPU_EVENT_CONCAT_(a, b) a##b
	#define GPU_EVENT_CONCAT(a, b) GPU_EVENT_CONCAT_(a, b)
	#define GPU_EVENT(cmdList, name) \
		prof::ScopedGPUEvent GPU_EVENT_CONCAT(gpuScopedEvent_, __COUNTER__)((cmdList), prof::eventID<name>)
#else
	#define GPU_EVENT(cmdList, name) ((void)(cmdList))
#endif // ENGINE_DEBUG_GPUEVENT

#if ENGINE_DEBUG_CPUEVENT
	class ScopedCPUEvent
	{
	public:
		ScopedCPUEvent(EVENT_INDEX nameID);
		~ScopedCPUEvent();

		ScopedCPUEvent(const ScopedCPUEvent&) = delete;
		ScopedCPUEvent& operator=(const ScopedCPUEvent&) = delete;

	private:
		int profEventIndex = -1;
	};

	#define CPU_EVENT_CONCAT_(a, b) a##b
	#define CPU_EVENT_CONCAT(a, b) CPU_EVENT_CONCAT_(a, b)
	#define CPU_EVENT(name) \
		prof::ScopedCPUEvent CPU_EVENT_CONCAT(cpuScopedEvent_, __COUNTER__)(prof::eventID<name>)
#else
	#define CPU_EVENT(name) ((void)0)
#endif // ENGINE_DEBUG_CPUEVENT

};  // namespace prof
