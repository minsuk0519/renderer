#include <render/render_memview.hpp>

#if ENGINE_DEBUG_MEMVIEW

#include <format>
#include <render/buffer.hpp>
#include <render/renderer.hpp>
#include <render/commandqueue.hpp>
#include <system/logger.hpp>

namespace render
{
	bool readbackResourceBytes(ID3D12Resource* src, uint size, D3D12_RESOURCE_STATES currentState, std::vector<unsigned char>& outBytes)
	{
		outBytes.clear();

		if (src == nullptr)
		{
			TC_LOG_ERROR("readbackResourceBytes: src is nullptr");
			return false;
		}

		if (size == 0)
		{
			TC_LOG_ERROR("readbackResourceBytes: size is 0");
			return false;
		}

		if (size > MEMVIEW_MAX_READBACK_BYTES)
		{
			TC_LOG_ERROR(std::format("readbackResourceBytes: size ({}) exceeds MEMVIEW_MAX_READBACK_BYTES ({})", size, MEMVIEW_MAX_READBACK_BYTES).c_str());
			return false;
		}

		D3D12_RESOURCE_DESC desc = src->GetDesc();

		if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			TC_LOG_ERROR("readbackResourceBytes: textures are unsupported; only buffers may be read back");
			return false;
		}

		if (size > desc.Width)
		{
			TC_LOG_ERROR(std::format("readbackResourceBytes: size ({}) exceeds resource width ({})", size, desc.Width).c_str());
			return false;
		}

		buffer* staging = e_globRenderer.debugSubsystem.getDebugReadBackBuffer();
		if (staging == nullptr)
		{
			return false;
		}

		commandqueue* queue = getCmdQueue(QUEUE_GRAPHIC);
		auto cmdList = queue->getCmdList();
		cmdList->Reset(queue->getAllocator().Get(), nullptr);

		bool needsBarrier = (currentState != D3D12_RESOURCE_STATE_COPY_SOURCE);

		if (needsBarrier)
		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(src, currentState, D3D12_RESOURCE_STATE_COPY_SOURCE);
			cmdList->ResourceBarrier(1, &barrier);
		}

		cmdList->CopyBufferRegion(staging->getResource(), 0, src, 0, size);

		if (needsBarrier)
		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(src, D3D12_RESOURCE_STATE_COPY_SOURCE, currentState);
			cmdList->ResourceBarrier(1, &barrier);
		}

		queue->execute({ cmdList });
		queue->flush();

		unsigned char* mapped = nullptr;
		if (!staging->mapReadbackBuffer(&mapped, size))
		{
			TC_LOG_ERROR("readbackResourceBytes: failed to map staging buffer");
			return false;
		}

		outBytes.resize(size);
		memcpy(outBytes.data(), mapped, size);
		staging->unmapReadbackBuffer();

		return true;
	}

	bool readbackBufferBytes(buffer* src, uint size, std::vector<unsigned char>& outBytes)
	{
		outBytes.clear();

		if (src == nullptr)
		{
			TC_LOG_ERROR("readbackBufferBytes: src is nullptr");
			return false;
		}

		return readbackResourceBytes(src->getResource(), size, src->getCurResourceState(), outBytes);
	}

}  // namespace render

#endif // ENGINE_DEBUG_MEMVIEW
