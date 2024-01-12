#include <render/renderer.hpp>
#include <render/commandqueue.hpp>
#include <render/pipelinestate.hpp>
#include <render/shader.hpp>
#include <render/rootsignature.hpp>
#include <render/transform.hpp>
#include <render/camera.hpp>

#include <system/logger.hpp>
#include <system/window.hpp>
#include <system/gui.hpp>

#include <d3dx12.h>

DirectX::XMFLOAT4 backgroundColor = DirectX::XMFLOAT4(0.8f, 0.9f, 0.9f, 1.0f);

uint vsync = 0;

renderer e_GlobRenderer;

bool renderer::init(Microsoft::WRL::ComPtr<IDXGIFactory4> dxFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter)
{
	factory = dxFactory;

	TC_CONDITIONB(createDevice(factory, adapter) == true, "Failed to create device");

	TC_INIT(shaders::loadResources());

	cmdqueue::allocateCmdQueue(device);

	TC_CONDITIONB(createSwapChain() == true, "Failed to create swapchain");

	cmdList = cmdqueue::createCommandList(cmdqueue::QUEUE_GRAPHIC, true);
	copyCmdList = cmdqueue::createCommandList(cmdqueue::QUEUE_COPY, true);
	computeCmdList = cmdqueue::createCommandList(cmdqueue::QUEUE_COMPUTE, true);

	TC_INIT(buf::loadResources(device, copyCmdList));

	descheap::loadResources(device);
	createFrameResources();

	root::initRootSignatures(device);
	pso::loadResources(device);

	//setting up gui
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> guiHeap;
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		TC_CONDITIONB(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&guiHeap)) == S_OK, "Failed to create DescriptorHeap");
	}

	TC_INIT(gui::init(e_globWindow.getWindow(), device.Get(), guiHeap.Get()));

	return true;
}

void renderer::close()
{

}

bool renderer::createDevice(Microsoft::WRL::ComPtr<IDXGIFactory4> dxFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter)
{
	factory = dxFactory;

	TC_CONDITIONB(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)) == S_OK, "Failed to create device");

#if defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(device.As(&pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		D3D12_MESSAGE_SEVERITY Severities[] = {
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		pInfoQueue->PushStorageFilter(&NewFilter);
	}
	else
	{
		return false;
	}
#endif

	return true;
}

bool renderer::checkFeatureSupport(DXGI_FEATURE feature)
{
	bool result = false;

	Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			if (FAILED(factory5->CheckFeatureSupport(feature, &result, sizeof(result)))) result = false;
		}
	}

	return result;
}

bool renderer::createSwapChain()
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = e_globWindow.width();
	swapChainDesc.Height = e_globWindow.height();
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = FRAME_COUNT;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	bool tearingSupport = checkFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING);
	if (!tearingSupport) TC_LOG_WARNING("Tearing is not supported on this device!");
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;// tearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
	HRESULT result = factory->CreateSwapChainForHwnd(
		cmdqueue::getCmdQueue(cmdqueue::QUEUE_GRAPHIC)->getQueue(),
		e_globWindow.getWindow(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1);

	TC_CONDITIONB(result == S_OK, "Failed to create SwapChain");

	factory->MakeWindowAssociation(e_globWindow.getWindow(), DXGI_MWA_NO_ALT_ENTER);
	swapChain1.As(&swapChain);

	return true;
}

bool renderer::createFrameResources()
{
	uint width = e_globWindow.width();
	uint height = e_globWindow.height();

	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		swapchainBuffer[i] = buf::createImageBuffer(width, height, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

		swapChain->GetBuffer(i, IID_PPV_ARGS(&swapchainBuffer[i]->resource));

		swapchainDesc[i] = descheap::getHeap(descheap::DESCRIPTORHEAP_RENDERTARGET)->requestdescriptor(buf::BUFFER_RT_TYPE, swapchainBuffer[i]);
	}

	return true;
}

void renderer::preDraw(float dt)
{

}

constantbuffer* projectionBuffer = nullptr;
descriptor desc;
float FAR_PLANE = 100.0f;
camera camObj;

void renderer::draw(float dt)
{
	auto frameBuffer = swapchainBuffer[frameIndex]->resource;
	auto cmdAllocator = cmdqueue::getCmdQueue(cmdqueue::QUEUE_GRAPHIC)->getAllocator();

	cmdAllocator->Reset();
	cmdList->Reset(cmdAllocator.Get(), pso::getpipelinestate(pso::PSO_PBR)->getPSO());

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(frameBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->ResourceBarrier(1, &barrier);

	renderScene();

	if(projectionBuffer == nullptr)
	{
		projectionBuffer = buf::createConstantBuffer(sizeof(float) * (4 * 4 * 2 + 4));

		desc = (descheap::getHeap(descheap::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_CONSTANT_TYPE, projectionBuffer));

		auto transformPtr = new transform();

		transformPtr->setPosition(DirectX::XMVECTOR{ 0.0f,0.0f,1.0f });

		DirectX::XMVECTOR rotation = transformPtr->getQuaternion();

		DirectX::XMVECTOR up = transformPtr->getUP();
		DirectX::XMVECTOR right = transformPtr->getRIGHT();

		DirectX::XMVECTOR forward = DirectX::XMVector3Cross(up, right);

		DirectX::XMVECTOR pos = transformPtr->getPosition();

		DirectX::XMMATRIX view = DirectX::XMMatrixLookToRH(pos, forward, up);

		DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(45.0f), e_globWindow.width() / (float)(e_globWindow.height()), 0.1f, FAR_PLANE);

		DirectX::XMMATRIX proj[2] = { projection, view };

		memcpy(projectionBuffer->info.cbvDataBegin, proj, sizeof(float) * 4 * 4 * 2);
		memcpy(projectionBuffer->info.cbvDataBegin + sizeof(float) * 4 * 4 * 2, &pos, sizeof(float) * 3);
		memcpy(projectionBuffer->info.cbvDataBegin + sizeof(float) * 4 * 4 * 2 + sizeof(float) * 3, &FAR_PLANE, sizeof(float));
		camObj.init();
	}

	{
		camObj.update(dt);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapchainDesc[frameIndex].getCPUHandle();

		root::getRootSignature(root::ROOT_PBR)->setRootSignature(cmdList);
		root::getRootSignature(root::ROOT_PBR)->registerDescHeap(cmdList);

		camObj.preDraw(cmdList, rtv);
		camObj.draw(0, cmdList);

		cmdList->IASetVertexBuffers(0, 1, &buf::getVertexBuffer(buf::VERTEX_CUBE)->view);
		cmdList->IASetVertexBuffers(1, 1, &buf::getVertexBuffer(buf::VERTEX_CUBE_NORM)->view);

		cmdList->IASetIndexBuffer(&buf::getIndexBuffer(buf::INDEX_CUBE)->view);

		cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0);
	}

	gui::render(cmdList.Get());

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(frameBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	cmdList->ResourceBarrier(1, &barrier);

	cmdqueue::getCmdQueue(cmdqueue::QUEUE_GRAPHIC)->execute({ cmdList });

	TC_CONDITION(swapChain->Present(vsync, DXGI_PRESENT_ALLOW_TEARING) == S_OK, "Failed to present the swapchain");

	//signal the queue graphics fence and wait for it.
	cmdqueue::getCmdQueue(cmdqueue::QUEUE_GRAPHIC)->flush();

	frameIndex = swapChain->GetCurrentBackBufferIndex();
}

void renderer::renderScene()
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapchainDesc[frameIndex].getCPUHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = D3D12_CPU_DESCRIPTOR_HANDLE(descheap::getHeap(descheap::DESCRIPTORHEAP_DEPTH)->getCPUPos(0));

	cmdList->ClearRenderTargetView(rtv, &backgroundColor.x, 0, nullptr);
	cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//draw may be called on here
	cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
}
