#include <render/framebuffer.hpp>
#include <system/logger.hpp>

#include <d3dx12.h>

bool framebuffer::createAddFBO(uint width, uint height, DXGI_FORMAT format, DirectX::XMFLOAT4 clear)
{
	clearColor = clear;

	buffer* FBO = e_globBufAllocator.alloc(nullptr, 0, 4, buf::GBF_RT | buf::GBF_SRV, buf::RESOURCE_CLEAR, format, width, height, 0, clearColor);

	FBOs.push_back(FBO);

	return true;
}

//open frame buffer and clear the buffer
//TODO we will add extra features for not clear color or depth
void framebuffer::openFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool clear)
{
	uint numFBO = FBOs.size();

	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;

	barriers.reserve(numFBO);
	rtvs.reserve(numFBO);

	for (uint i = 0; i < numFBO; ++i) barriers.push_back(FBOs[i]->getTransition(D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	cmdList->ResourceBarrier(barriers.size(), barriers.data());

	for (uint i = 0; i < numFBO; ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = FBOs[i]->getDesc(buf::GBF_RT)->getCPUHandle();

		rtvs.push_back(cpuHandle);

		if(clear) cmdList->ClearRenderTargetView(rtvs[i], &clearColor.x, 0, nullptr);
	}

	if (isDepth)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = D3D12_CPU_DESCRIPTOR_HANDLE(render::getHeap(render::DESCRIPTORHEAP_DEPTH)->getCPUPos(0));

		if(clear) cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, clearDepth, 0, 0, nullptr);
		
		cmdList->OMSetRenderTargets(numFBO, rtvs.data(), FALSE, &dsv);
	}
	else
	{
		cmdList->OMSetRenderTargets(numFBO, rtvs.data(), FALSE, nullptr);
	}
}

//close frame buffer and perform transition for image buffer
void framebuffer::closeFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
	for (auto FBO : FBOs)
	{
		barriers.push_back(FBO->getTransition(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	}

	cmdList->ResourceBarrier(barriers.size(), barriers.data());
}

void framebuffer::setDepthClear(float depth)
{
	clearDepth = depth;
	isDepth = true;
}

D3D12_GPU_DESCRIPTOR_HANDLE framebuffer::getDescHandle(uint FBOIndex)
{
	TC_ASSERT(FBOIndex < FBOs.size());

	return FBOs[FBOIndex]->getDesc(buf::GBF_SRV)->getHandle();
}