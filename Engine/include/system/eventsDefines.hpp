#pragma once

#include <system/defines.hpp>

#if ENGINE_DEBUG_EVENT
namespace prof
{
	enum EVENT_INDEX : int { EVENT_INVALID = -1 };

	constexpr int EVENT_CAPACITY = 512;

	struct eventRegistry
	{
		const char* names[EVENT_CAPACITY];
		int count;
	};

	inline eventRegistry& getEventRegistry()
	{
		static eventRegistry reg{};
		return reg;
	}

	inline EVENT_INDEX registerEvent(const char* name)
	{
		eventRegistry& reg = getEventRegistry();
		if (reg.count >= EVENT_CAPACITY)
		{
			return EVENT_INVALID;
		}
		reg.names[reg.count] = name;
		return static_cast<EVENT_INDEX>(reg.count++);
	}

	template<unsigned N>
	struct eventName
	{
		char value[N];
		constexpr eventName(const char (&s)[N])
		{
			for (unsigned i = 0; i < N; ++i)
			{
				value[i] = s[i];
			}
		}
	};

	template<eventName S>
	inline const EVENT_INDEX eventID = registerEvent(S.value);

	inline const char* getEventName(EVENT_INDEX id)
	{
		eventRegistry& reg = getEventRegistry();
		if (id >= 0 && id < reg.count)
		{
			return reg.names[id];
		}
		return "UnknownEvent";
	}

	inline int getEventCount()
	{
		return getEventRegistry().count;
	}

}  // namespace prof
#endif // #if ENGINE_DEBUG_EVENT
