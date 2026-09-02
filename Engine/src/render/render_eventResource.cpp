#include <render/render_eventResource.hpp>
#include <render/commandqueue.hpp>
#include <render/buffer.hpp>
#include <system/logger.hpp>

#if ENGINE_DEBUG_EVENTRESOURCE

namespace render
{
	void flushEventResourceScopeBackend(void* cmdList, prof::EVENT_INDEX nameID)
	{
		ID3D12GraphicsCommandList* list = static_cast<ID3D12GraphicsCommandList*>(cmdList);
		D3D12_COMMAND_LIST_TYPE listType = list->GetType();

		QUEUE_INDEX queueIdx = queueIndexFromListType(listType);
		if (queueIdx >= QUEUE_MAX)
		{
			return;  // Unsupported list type
		}

		commandqueue* queue = getCmdQueue(queueIdx);
		if (queue)
		{
			queue->drainBindScratchToEvent(nameID);
		}
	}

	const char* getEventResourceNameBackend(uint bufferId)
	{
		#if ENGINE_DEBUG_RESOURCEVIEW
		return buf::getResourceDisplayName(bufferId);
		#else
		return "resource";
		#endif
	}

	void setResourceViewerSelectionBackend(uint bufferId)
	{
		#if ENGINE_DEBUG_RESOURCEVIEW
		buf::setSelectedResourceId(bufferId);
		#else
		(void)bufferId;
		#endif
	}

}  // namespace render

#endif // ENGINE_DEBUG_EVENTRESOURCE
