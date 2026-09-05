#pragma once

#include <system/defines.hpp>
#include <vector>

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
#endif // ENGINE_DEBUG_READBACK
}  // namespace render

#if ENGINE_DEBUG_READBACK
class renderDebug
{
public:
	void update();
	buffer* getDebugReadBackBuffer();

#if ENGINE_DEBUG_MEMVIEW && ENGINE_DEBUG_RESOURCEVIEW
	void guiMemoryReadbackSetting();
#endif // ENGINE_DEBUG_MEMVIEW && ENGINE_DEBUG_RESOURCEVIEW

private:
	void ensureDebugReadBackBuffer();
	buffer* debugReadBackBuffer = nullptr;
	bool debugReadBackBufferAttempted = false;

#if ENGINE_DEBUG_MEMVIEW
	void requestMemReadback(buffer* target, uint byteCount);
	const std::vector<unsigned char>& getMemReadbackData() const;
	uint getMemReadbackResultId() const;
	bool getMemReadbackFailed() const;

	bool memReadbackRequest = false;
	buffer* memReadbackTarget = nullptr;
	uint memReadbackByteCount = 0;
	std::vector<unsigned char> memReadbackData;
	uint memReadbackResultId = ~0u;
	bool memReadbackFailed = false;
#endif // ENGINE_DEBUG_MEMVIEW
};

#endif // ENGINE_DEBUG_READBACK
