#pragma once

#include <system/defines.hpp>

#if ENGINE_DEBUG_GPUEVENT
namespace render
{
	enum GPUEVENT_INDEX : int { GPUEVENT_INVALID = -1 };

	constexpr int GPUEVENT_CAPACITY = 256;

	struct gpuEventRegistry
	{
		const char* names[GPUEVENT_CAPACITY];
		int count;
	};

	inline gpuEventRegistry& getGPUEventRegistry()
	{
		static gpuEventRegistry reg{};
		return reg;
	}

	inline GPUEVENT_INDEX registerGPUEvent(const char* name)
	{
		gpuEventRegistry& reg = getGPUEventRegistry();
		if (reg.count >= GPUEVENT_CAPACITY)
		{
			return GPUEVENT_INVALID;
		}
		reg.names[reg.count] = name;
		return static_cast<GPUEVENT_INDEX>(reg.count++);
	}

	template<unsigned N>
	struct gpuEventName
	{
		char value[N];
		constexpr gpuEventName(const char (&s)[N])
		{
			for (unsigned i = 0; i < N; ++i)
			{
				value[i] = s[i];
			}
		}
	};

	template<gpuEventName S>
	inline const GPUEVENT_INDEX gpuEventID = registerGPUEvent(S.value);

	inline const char* getGPUEventName(GPUEVENT_INDEX id)
	{
		gpuEventRegistry& reg = getGPUEventRegistry();
		if (id >= 0 && id < reg.count)
		{
			return reg.names[id];
		}
		return "UnknownGPUEvent";
	}

	inline int getGPUEventCount()
	{
		return getGPUEventRegistry().count;
	}

}  // namespace render
#endif // #if ENGINE_DEBUG_GPUEVENT
