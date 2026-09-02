#include <render/commandqueue.hpp>
#include <system/logger.hpp>
#include <render/renderer.hpp>
#include <render/rootsignature.hpp>

#include <array>
#include <string>

namespace render
{
	const D3D12_COMMAND_LIST_TYPE queuetypes[] = {
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		D3D12_COMMAND_LIST_TYPE_COMPUTE,
		D3D12_COMMAND_LIST_TYPE_COPY,
	};

	TC_STATICASSERT(static_cast<int>(sizeof(queuetypes) / sizeof(D3D12_COMMAND_LIST_TYPE)) == QUEUE_MAX);

#if ENGINE_DEBUG_EVENTRESOURCE
	TC_STATICASSERT(QUEUE_MAX == 3);
#endif // ENGINE_DEBUG_EVENTRESOURCE

	QUEUE_INDEX queueIndexFromListType(D3D12_COMMAND_LIST_TYPE type)
	{
		if (type == D3D12_COMMAND_LIST_TYPE_DIRECT)
		{
			return QUEUE_GRAPHIC;
		}
		else if (type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
		{
			return QUEUE_COMPUTE;
		}
		else if (type == D3D12_COMMAND_LIST_TYPE_COPY)
		{
			return QUEUE_COPY;
		}
		return QUEUE_MAX;
	}

	std::array<commandqueue*, QUEUE_MAX> cmdQueues;

	commandqueue* getCmdQueue(const QUEUE_INDEX index)
	{
		return cmdQueues[index];
	}

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> createCommandList(const render::QUEUE_INDEX& queueType, bool close)
	{
		return cmdQueues[queueType]->createCommandList(close);
	}

	bool allocateCmdQueue()
	{
		for (uint type = 0; type < QUEUE_MAX; ++type)
		{
			commandqueue* newQueue = new commandqueue();
			newQueue->init(queuetypes[type]);
			cmdQueues[type] = newQueue;
		}

		return true;
	}

	void closeCmdQueue()
	{
		for (uint i = 0; i < QUEUE_MAX; ++i)
		{
			delete cmdQueues[i];
		}
	}
}

ID3D12CommandQueue* commandqueue::getQueue() const
{
	return commandQueue.Get();
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandqueue::getAllocator() const
{
	return commandAllocator;
}

bool commandqueue::init(const D3D12_COMMAND_LIST_TYPE commandType)
{	
	this->type = commandType;

	createCommandAllocator();

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	TC_CONDITIONB(e_globRenderer.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)) == S_OK, "Failed to create command queue");
	createFence();

	createCommandList(true);

	return true;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandqueue::getCmdList() const
{
	return commandList;
}

void commandqueue::execute(const std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> cmdLists)// const std::vector<ID3D12CommandList*> cmdLists)
{
	std::vector<ID3D12CommandList*> lists;

	for (Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmd : cmdLists)
	{
		HRESULT hr = cmd->Close();

		if (FAILED(hr))
		{
			std::string errorMsg = std::format("Failed to close cmd!");
			TC_LOG_ERROR(errorMsg.c_str());
		}

		lists.push_back(cmd.Get());
	}

	commandQueue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
}

bool commandqueue::createCommandAllocator()
{
	TC_CONDITIONB(e_globRenderer.device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)) == S_OK, "Failed to create CommandAllocator");
	
	return true;
}

bool commandqueue::createFence()
{
	TC_CONDITIONB(e_globRenderer.device->CreateFence(fence.fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.fence)) == S_OK, "Failed to create fence");

	HANDLE result = CreateEvent(NULL, FALSE, FALSE, NULL);
	TC_CONDITION(result != NULL, "Failed to create event");

	fenceEvent = result;

	return true;
}

void commandqueue::flush()
{
	++fence.fenceValue;

	TC_CONDITION(commandQueue->Signal(fence.fence.Get(), fence.fenceValue) == S_OK, "Failed to signal fence");

	if (fence.fence->GetCompletedValue() < fence.fenceValue)
	{
		fence.fence->SetEventOnCompletion(fence.fenceValue, fenceEvent);

		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandqueue::createCommandList(bool close)
{
	TC_CONDITION(e_globRenderer.device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)) == S_OK, "Failed to create CommandList");

	if (close) TC_CONDITION(commandList->Close() == S_OK, "Failed to close CommandList");

	return commandList;
}


void commandqueue::signalFence()
{
}

void commandqueue::bindPSO(render::PSO_INDEX psoIndex)
{
	currentPSO = render::getpipelinestate(psoIndex);

	commandAllocator->Reset();
	commandList->Reset(commandAllocator.Get(), currentPSO->getPSO());

	rootsignature* rootsig = currentPSO->getRootSig();

	rootsig->setRootSignature(commandList);
	rootsig->registerDescHeap(commandList);

#if ENGINE_DEBUG_EVENTRESOURCE
	indirectBindScratch.clear();
#endif // ENGINE_DEBUG_EVENTRESOURCE
}

void commandqueue::sendData(uint pos, buffer* buf, buf::graphicBufferFlags viewType)
{
	descriptor* desc = buf->getDesc(viewType);

	currentPSO->sendGraphicsData(commandList, pos, desc->getHandle());

#if ENGINE_DEBUG_EVENTRESOURCE
	if (desc->heapIndex == render::DESCRIPTORHEAP_BUFFER && pos < render::EVENTRESOURCE_MAX_LOC)
	{
		bindScratch.locs[pos] = { buf->getId(), (uint16_t)desc->heapOffset };
		if (indirectBindScratch.count < render::EVENTRESOURCE_MAX_LOC)
		{
			indirectBindScratch.indices[indirectBindScratch.count++] = (uint16_t)pos;
		}
		else if (!indirectBindScratch.warnedFull)
		{
			TC_LOG_WARNING("indirectBindScratch table full");
			indirectBindScratch.warnedFull = true;
		}
	}
#endif // ENGINE_DEBUG_EVENTRESOURCE
}

void commandqueue::sendData(uint pos, uint size, void* data)
{
	currentPSO->sendConstantData(commandList, pos, size, data);
}

#if ENGINE_DEBUG_EVENTRESOURCE
void commandqueue::drainBindScratchToEvent(prof::EVENT_INDEX nameID)
{
	if (indirectBindScratch.count == 0)
	{
		return;
	}

	for (uint i = 0; i < indirectBindScratch.count; ++i)
	{
		uint16_t pos = indirectBindScratch.indices[i];
		const render::bindScratchEntry& entry = bindScratch.locs[pos];

		if (currentPSO != nullptr && currentPSO->usesHlslLoc(pos))
		{
			prof::accumulateEventResource(nameID, entry.bufferId, pos, entry.heapSlot);
		}
	}

	indirectBindScratch.count = 0;
	indirectBindScratch.warnedFull = false;
}
#endif // ENGINE_DEBUG_EVENTRESOURCE

