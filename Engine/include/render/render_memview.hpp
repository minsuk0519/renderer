#pragma once

#include <system/defines.hpp>
#include <render/render_debug.hpp>
#include <d3d12.h>
#include <vector>

struct buffer;

#if ENGINE_DEBUG_MEMVIEW

namespace render
{
	constexpr uint MEMVIEW_MAX_READBACK_BYTES = render::DEBUG_READBACK_MEMVIEW_BYTES;

	bool readbackResourceBytes(ID3D12Resource* src, uint size, D3D12_RESOURCE_STATES currentState, std::vector<unsigned char>& outBytes);
	bool readbackBufferBytes(buffer* src, uint size, std::vector<unsigned char>& outBytes);

}  // namespace render

#endif // ENGINE_DEBUG_MEMVIEW
