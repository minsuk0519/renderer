#include <render/framebuffer.hpp>

#include <d3dx12.h>
#include <DirectXMath.h>


DirectX::XMFLOAT4 backgroundColor = DirectX::XMFLOAT4(0.8f, 0.9f, 0.9f, 1.0f);

bool framebuffer::createFB(uint width, uint height, uint frameNum, Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain)
{
	imageBuffer = buf::createImageBuffer(width, height, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

	swapChain->GetBuffer(frameNum, IID_PPV_ARGS(&imageBuffer->resource));

	desc = render::getHeap(render::DESCRIPTORHEAP_RENDERTARGET)->requestdescriptor(buf::BUFFER_RT_TYPE, imageBuffer);

	return true;
}

//open frame buffer and clear the buffer
//TODO we will add extra features for not clear color or depth
void framebuffer::openFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(imageBuffer->resource.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->ResourceBarrier(1, &barrier);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = desc.getCPUHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = D3D12_CPU_DESCRIPTOR_HANDLE(render::getHeap(render::DESCRIPTORHEAP_DEPTH)->getCPUPos(0));

	cmdList->ClearRenderTargetView(rtv, &backgroundColor.x, 0, nullptr);
	cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//draw may be called on here
	cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
}

//close frame buffer and perform transition for image buffer
void framebuffer::closeFB(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(imageBuffer->resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	cmdList->ResourceBarrier(1, &barrier);
}