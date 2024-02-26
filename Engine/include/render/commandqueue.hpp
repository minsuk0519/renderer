#pragma once

#include <system/defines.hpp>
#include <render/pipelinestate.hpp>

#include <wrl.h>
#include <d3d12.h>

#include <vector>

class commandqueue;
class pipelinestate;

namespace render
{
	struct fence
	{
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;

		uint fenceValue = 0;
	};

	enum QUEUE_INDEX
	{
		QUEUE_GRAPHIC,
		QUEUE_COMPUTE,
		QUEUE_COPY,
		QUEUE_MAX,
	};

	commandqueue* getCmdQueue(const QUEUE_INDEX index);
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> createCommandList(const render::QUEUE_INDEX& queueType, bool close);

	bool allocateCmdQueue();
	void closeCmdQueue();
};

class commandqueue
{
private:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

	render::fence fence;
	HANDLE fenceEvent;

	D3D12_COMMAND_LIST_TYPE type;

	pipelinestate* currentPSO = nullptr;

private:
	bool createCommandAllocator();
	bool createFence();
public:
	ID3D12CommandQueue* getQueue() const;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> getAllocator() const;

	bool init(const D3D12_COMMAND_LIST_TYPE commandType);

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> getCmdList() const;

	void execute(const std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> cmdLists);
	void flush();

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> createCommandList(bool close);
	void signalFence();

	void bindPSO(render::PSO_INDEX psoIndex);
	void sendData(uint pos, D3D12_GPU_DESCRIPTOR_HANDLE descLoc);
};