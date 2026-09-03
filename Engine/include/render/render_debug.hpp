#pragma once

#include <system/defines.hpp>

struct buffer;

namespace render
{
#if ENGINE_DEBUG_READBACK
	constexpr uint DEBUG_READBACK_STATS_BYTES = sizeof(uint) * 32;
	constexpr uint DEBUG_READBACK_MEMVIEW_BYTES = 64u * 1024u * 1024u;
#if ENGINE_DEBUG_MEMVIEW
	constexpr uint DEBUG_READBACK_BUFFER_SIZE = DEBUG_READBACK_MEMVIEW_BYTES;
#else
	constexpr uint DEBUG_READBACK_BUFFER_SIZE = DEBUG_READBACK_STATS_BYTES;
#endif // ENGINE_DEBUG_MEMVIEW
	buffer* getDebugReadBackBuffer();
#endif // ENGINE_DEBUG_READBACK
}  // namespace render
