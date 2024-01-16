#pragma once

#include <system/defines.hpp>
#include <render/buffer.hpp>

#include <wrl.h>
#include <d3d12.h>

#include <array>
#include <vector>

class descriptorheap;

namespace render
{
	enum descriptorHeapIndex
	{
		DESCRIPTORHEAP_RENDERTARGET = 0,
		DESCRIPTORHEAP_BUFFER,
		DESCRIPTORHEAP_DEPTH,
		DESCRIPTORHEAP_MAX,
	};

	struct descHeapCache
	{
		D3D12_DESCRIPTOR_HEAP_TYPE Type;
		UINT NumDescriptors;
		D3D12_DESCRIPTOR_HEAP_FLAGS Flags;
	};

	descriptorheap* getHeap(uint index);

	bool initDescHeap();
	void cleanUpDescHeap();

	void createSampler(descriptorHeapIndex heapIndex, uint offset);
}

struct descriptor
{
	uint heapOffset;
	render::descriptorHeapIndex heapIndex;

	D3D12_GPU_DESCRIPTOR_HANDLE getHandle() const;
	D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle() const;
};

class descriptorheap
{
private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
	uint incrementalSize = 0;
	uint numDescriptor;

	std::array<std::vector<descriptor>, render::DESCRIPTORHEAP_MAX> occupiedDescriptor;

public:
	bool init(D3D12_DESCRIPTOR_HEAP_TYPE type, uint count, D3D12_DESCRIPTOR_HEAP_FLAGS flags);

	unsigned __int64 getGPUPos(uint index = 0);
	unsigned __int64 getCPUPos(uint index = 0);

	void updateData(void* data, uint size, uint index = 0);

	ID3D12DescriptorHeap* getHeap() const;

	uint setRootTable(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, uint offset);

	descriptor requestdescriptor(const buf::BUFFER_TYPE type, buffer* buf);

	uint getRemainPos(render::descriptorHeapIndex heapIdx) const;
};

