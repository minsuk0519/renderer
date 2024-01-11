#pragma once

#include <system/defines.hpp>

#include <wrl.h>
#include <d3d12.h>

#include <vector>

class commandqueue;

namespace cmdqueue
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
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> createCommandList(const cmdqueue::QUEUE_INDEX& queueType, bool close);

	bool allocateCmdQueue(Microsoft::WRL::ComPtr<ID3D12Device2> dxdevice);
	void closeCmdQueue();
};

class commandqueue
{
private:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;

	cmdqueue::fence fence;
	HANDLE fenceEvent;

	D3D12_COMMAND_LIST_TYPE type;

private:
	bool createCommandAllocator();
	bool createFence();
public:
	ID3D12CommandQueue* getQueue() const;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> getAllocator() const;

	bool init(const D3D12_COMMAND_LIST_TYPE commandType);

	void execute(const std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> cmdLists);//const std::vector<ID3D12CommandList*> cmdLists);
	void flush();

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> createCommandList(bool close);
	void signalFence();
};