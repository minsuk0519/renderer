#include <render/descriptorheap.hpp>
#include <system/logger.hpp>

#include <render/renderer.hpp>

constexpr uint MAX_DESCRIPTOR_SIZE = 1024;

namespace render
{
	std::array<descriptorheap*, DESCRIPTORHEAP_MAX> descriptorHeaps;

	descriptorheap* getHeap(uint index)
	{
		return descriptorHeaps[index];
	}

	bool initDescHeap()
	{
		//create rendertarget rtv
		{
			descriptorheap* newHeap = new descriptorheap();
			newHeap->init(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_DESCRIPTOR_SIZE, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

			descriptorHeaps[DESCRIPTORHEAP_RENDERTARGET] = newHeap;
		}

		{
			descriptorheap* newHeap = new descriptorheap();
			newHeap->init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_DESCRIPTOR_SIZE, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
			//newHeap->register_data({ buf::CONSTANT_PROJECTION , buf::CONSTANT_OBJECT , buf::CONSTANT_SUN , buf::CONSTANT_HAMRAN , buf::IMAGE_IRRADIANCE , buf::IMAGE_ENVIRONMENT, buf::CONSTANT_OBJECT2 });

			descriptorHeaps[DESCRIPTORHEAP_BUFFER] = newHeap;
		}

		{
			descriptorheap* newHeap = new descriptorheap();
			newHeap->init(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MAX_DESCRIPTOR_SIZE, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

			//e_globRenderer.device->CreateDepthStencilView(buf::getDepthBuffer(buf::DEPTH_SWAPCHAIN)->resource.Get(), &buf::getDepthBuffer(buf::DEPTH_SWAPCHAIN)->view, D3D12_CPU_DESCRIPTOR_HANDLE(newHeap->getCPUPos(0)));

			descriptorHeaps[DESCRIPTORHEAP_DEPTH] = newHeap;
		}

		return true;
	}

	void cleanUpDescHeap()
	{
		for (uint i = 0; i < DESCRIPTORHEAP_MAX; ++i)
		{
			delete descriptorHeaps[i];
		}
	}

	void createSampler(descriptorHeapIndex heapIndex, uint offset)
	{
		//CD3DX12_STATIC_SAMPLER_DESC samplerDesc(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);
		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		e_globRenderer.device->CreateSampler(&samplerDesc, D3D12_CPU_DESCRIPTOR_HANDLE(descriptorHeaps[heapIndex]->getCPUPos(offset)));
	}
}


bool descriptorheap::init(D3D12_DESCRIPTOR_HEAP_TYPE type, uint count, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
	numDescriptor = count;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = count;
	desc.Type = type;
	desc.Flags = flags;

	TC_CONDITIONB(e_globRenderer.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)) == S_OK, "Failed to create DescriptorHeap");

	incrementalSize = e_globRenderer.device->GetDescriptorHandleIncrementSize(type);

	return true;
}

unsigned __int64 descriptorheap::getGPUPos(uint index)
{
	unsigned __int64 result = heap->GetGPUDescriptorHandleForHeapStart().ptr;
	result += index * incrementalSize;
	return result;
}

unsigned __int64 descriptorheap::getCPUPos(uint index)
{
	unsigned __int64 result = heap->GetCPUDescriptorHandleForHeapStart().ptr;
	result += index * incrementalSize;
	return result;
}

void descriptorheap::updateData(void* data, uint size, uint index)
{
	memcpy(reinterpret_cast<void*>(getGPUPos(index)), data, size);
}

ID3D12DescriptorHeap* descriptorheap::getHeap() const
{
	return heap.Get();
}

//TODO : change later
uint descriptorheap::setRootTable(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, uint offset)
{
	uint off = offset;

	//for (uint i = 0; i < constantData.size(); ++i)
	//{
	//	cmdList->SetGraphicsRootDescriptorTable(off, (D3D12_GPU_DESCRIPTOR_HANDLE)getGPUPos(i));
	//	++off;
	//}

	return off;
}

template <typename T>
T* getCorrespondingView(void* ptr)
{
	return reinterpret_cast<T*>(ptr);
}

descriptor descriptorheap::requestdescriptor(const buf::BUFFER_TYPE type, buffer* buf, void* view)
{
	render::descriptorHeapIndex heapIdx;

	if (type == buf::BUFFER_DEPTH_TYPE) heapIdx = render::DESCRIPTORHEAP_DEPTH;
	else if (type == buf::BUFFER_RT_TYPE) heapIdx = render::DESCRIPTORHEAP_RENDERTARGET;
	else heapIdx = render::DESCRIPTORHEAP_BUFFER;

	uint pos = getRemainPos(heapIdx);

	descriptor result;
	result.heapOffset = pos;
	result.heapIndex = heapIdx;

	occupiedDescriptor[heapIdx].push_back(result);

	//we create buffer later
	if (buf == nullptr) return result;

	switch (type)
	{
	case buf::BUFFER_CONSTANT_TYPE:
	{	
		e_globRenderer.device->CreateConstantBufferView(getCorrespondingView<D3D12_CONSTANT_BUFFER_VIEW_DESC>(view), D3D12_CPU_DESCRIPTOR_HANDLE(getCPUPos(pos)));
	}
	break;
	case buf::BUFFER_DEPTH_TYPE:
	{
		e_globRenderer.device->CreateDepthStencilView(buf->getResource(), getCorrespondingView<D3D12_DEPTH_STENCIL_VIEW_DESC>(view), D3D12_CPU_DESCRIPTOR_HANDLE(getCPUPos(pos)));
	}
	break;
	case buf::BUFFER_IMAGE_TYPE:
	{
		e_globRenderer.device->CreateShaderResourceView(buf->getResource(), getCorrespondingView<D3D12_SHADER_RESOURCE_VIEW_DESC>(view), D3D12_CPU_DESCRIPTOR_HANDLE(getCPUPos(pos)));
	}
	break;
	case buf::BUFFER_UAV_TYPE:
	{
		e_globRenderer.device->CreateUnorderedAccessView(buf->getResource(), nullptr, getCorrespondingView<D3D12_UNORDERED_ACCESS_VIEW_DESC>(view), D3D12_CPU_DESCRIPTOR_HANDLE(getCPUPos(pos)));
	}
	break;
	case buf::BUFFER_RT_TYPE:
	{
		e_globRenderer.device->CreateRenderTargetView(buf->getResource(), getCorrespondingView<D3D12_RENDER_TARGET_VIEW_DESC>(view), D3D12_CPU_DESCRIPTOR_HANDLE(getCPUPos(pos)));
	}
	break;
	default:
		TC_LOG_WARNING("Unidentified constant index!");
		break;
	}

	return result;
}

uint descriptorheap::getRemainPos(render::descriptorHeapIndex heapIdx) const
{
	uint result = 0;

	for (auto occupied : occupiedDescriptor[heapIdx])
	{
		if (occupied.heapOffset != result)
		{
			return result;
		}

		++result;
	}

	return result;
}

D3D12_GPU_DESCRIPTOR_HANDLE descriptor::getHandle() const
{
	//will be not only for the descriptorheap buffer
	return D3D12_GPU_DESCRIPTOR_HANDLE(render::descriptorHeaps[heapIndex]->getGPUPos(heapOffset));
}

D3D12_CPU_DESCRIPTOR_HANDLE descriptor::getCPUHandle() const
{

	return D3D12_CPU_DESCRIPTOR_HANDLE(render::descriptorHeaps[heapIndex]->getCPUPos(heapOffset));
}
