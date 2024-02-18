#include <render/framebuffer.hpp>

#include <d3dx12.h>

bool framebuffer::createAddFBO(uint width, uint height, DXGI_FORMAT format, DirectX::XMFLOAT4 clearColor)
{
	framebufferObject* FBO = new framebufferObject();

	FBO->imageBuffer = buf::createImageBuffer(width, height, 1, format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	FBO->desc = render::getHeap(render::DESCRIPTORHEAP_RENDERTARGET)->requestdescriptor(buf::BUFFER_RT_TYPE, FBO->imageBuffer);
	FBO->textureDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, FBO->imageBuffer);

	FBO->clearColor = clearColor;

	FBOs.push_back(FBO);

	return true;
}

void framebuffer::addFBOfromBuf(Microsoft::WRL::ComPtr<ID3D12Resource>& resource, DirectX::XMFLOAT4 clearColor)
{
	framebufferObject* FBO = new framebufferObject();

	FBO->imageBuffer = new imagebuffer;

	FBO->imageBuffer->resource = resource;

	FBO->imageBuffer->view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	FBO->imageBuffer->view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	FBO->imageBuffer->view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	FBO->imageBuffer->view.Texture2D.MipLevels = 1;

	FBO->desc = render::getHeap(render::DESCRIPTORHEAP_RENDERTARGET)->requestdescriptor(buf::BUFFER_RT_TYPE, FBO->imageBuffer);
	FBO->textureDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, FBO->imageBuffer);

	FBO->clearColor = clearColor;

	FBOs.push_back(FBO);
}

//open frame buffer and clear the buffer
//TODO we will add extra features for not clear color or depth
void framebuffer::openFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	uint numFBO = FBOs.size();

	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;

	barriers.reserve(numFBO);
	rtvs.reserve(numFBO);

	for (uint i = 0; i < numFBO; ++i) barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			FBOs[i]->imageBuffer->resource.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
		));

	cmdList->ResourceBarrier(barriers.size(), barriers.data());

	for (uint i = 0; i < numFBO; ++i)
	{
		rtvs.push_back(FBOs[i]->desc.getCPUHandle());

		cmdList->ClearRenderTargetView(rtvs[i], &FBOs[i]->clearColor.x, 0, nullptr);
	}

	if (isDepth)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = D3D12_CPU_DESCRIPTOR_HANDLE(render::getHeap(render::DESCRIPTORHEAP_DEPTH)->getCPUPos(0));

		cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, clearDepth, 0, 0, nullptr);
		
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
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(FBO->imageBuffer->resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
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
	assert(FBOIndex < FBOs.size());

	return FBOs[FBOIndex]->textureDesc.getHandle();
}
