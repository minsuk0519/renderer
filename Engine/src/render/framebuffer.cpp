#include <render/framebuffer.hpp>
#include <system/logger.hpp>

#include <d3dx12.h>

bool framebuffer::createAddFBO(uint width, uint height, DXGI_FORMAT format, DirectX::XMFLOAT4 clear)
{
	clearColors.push_back(clear);

	buffer* FBO = e_globBufAllocator.alloc(nullptr, width * height, 4, buf::GBF_RT | buf::GBF_SRV, buf::RESOURCE_CLEAR | buf::RESOURCE_TEXTURE, format, width, height, 1, clear);

	FBOs.push_back(FBO);

	return true;
}

bool framebuffer::attachResource(ID3D12Resource* resource, uint width, uint height, DXGI_FORMAT format, DirectX::XMFLOAT4 clear)
{
	clearColors.push_back(clear);

	buffer* FBO = e_globBufAllocator.alloc(nullptr, width * height, 4, buf::GBF_RT, 0, format, width, height, 0, clear, resource);

	FBOs.push_back(FBO);

	return true;
}

bool framebuffer::attachDepth(buffer* depth, float clear)
{
	depthBuffer = depth;
	clearDepth = clear;

	return false;
}

void framebuffer::openFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool clear)
{
	uint numFBO = (uint)FBOs.size();

	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;

	barriers.reserve(numFBO);
	rtvs.reserve(numFBO);

	for (uint i = 0; i < numFBO; ++i) barriers.push_back(FBOs[i]->getTransition(D3D12_RESOURCE_STATE_RENDER_TARGET));

	cmdList->ResourceBarrier((uint)barriers.size(), barriers.data());

	for (uint i = 0; i < numFBO; ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = FBOs[i]->getDesc(buf::GBF_RT)->getCPUHandle();

		rtvs.push_back(cpuHandle);

		if(clear) cmdList->ClearRenderTargetView(rtvs[i], &clearColors[i].x, 0, nullptr);
	}

	if (depthBuffer)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = D3D12_CPU_DESCRIPTOR_HANDLE(depthBuffer->getDesc(buf::GBF_DEPTH_STENCIL)->getCPUHandle());

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
		barriers.push_back(FBO->getTransition(D3D12_RESOURCE_STATE_PRESENT));
	}

	cmdList->ResourceBarrier((uint)barriers.size(), barriers.data());
}

D3D12_GPU_DESCRIPTOR_HANDLE framebuffer::getDescHandle(uint FBOIndex)
{
	TC_ASSERT(FBOIndex < FBOs.size());

	return FBOs[FBOIndex]->getDesc(buf::GBF_SRV)->getHandle();
}

ID3D12Resource* framebuffer::getFBOResource(uint FBOIndex)
{
	TC_ASSERT(FBOIndex < FBOs.size());

	return FBOs[FBOIndex]->getResource();
}