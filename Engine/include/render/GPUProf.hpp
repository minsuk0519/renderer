#pragma once

#include <system/defines.hpp>
#include <render/gpuEventsDefines.hpp>

#if ENGINE_DEBUG_GPUPROF

#include <d3d12.h>

namespace render
{
	bool initGPUProf();
	void closeGPUProf();
	void endGPUProfFrame();
	int  beginGPUProfEvent(ID3D12GraphicsCommandList* cmdList, GPUEVENT_INDEX nameID);
	void endGPUProfEvent(ID3D12GraphicsCommandList* cmdList, int eventIndex);

}  // namespace render

#endif // ENGINE_DEBUG_GPUPROF
