#include <render/renderer.hpp>
#include <render/commandqueue.hpp>
#include <render/pipelinestate.hpp>
#include <render/shader.hpp>
#include <render/rootsignature.hpp>
#include <render/transform.hpp>
#include <render/camera.hpp>
#include <render/mesh.hpp>
#include <render/framebuffer.hpp>
#include <world/world.hpp>

#include <system/logger.hpp>
#include <system/window.hpp>
#include <system/gui.hpp>

#include <d3dx12.h>

//TODO
uint vsync = 0;

uint frameIndex = 0;

renderer e_globRenderer;

bool initGui()
{
	//setting up gui
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> guiHeap;
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (e_globRenderer.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&guiHeap)) != S_OK)
		{
			return false;
		}
	}

	gui::init(e_globWindow.getWindow(), e_globRenderer.device.Get(), guiHeap.Get());
}

bool renderer::init(Microsoft::WRL::ComPtr<IDXGIFactory4> dxFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter)
{
	factory = dxFactory;

	TC_CONDITIONB(createDevice(factory, adapter) == true, "Failed to create device");
	TC_INIT(shaders::loadResources());
	TC_INIT(render::allocateCmdQueue());
	TC_CONDITIONB(createSwapChain() == true, "Failed to create swapchain");
	TC_INIT(buf::loadResources());
	TC_INIT(render::initDescHeap());
	TC_INIT(createFrameResources());
	TC_INIT(render::initPSO());
	TC_INIT(msh::loadResources());

	TC_INIT(initGui());

	return true;
}

void renderer::close()
{
	msh::cleanUp();

	gui::close();
	render::cleanUpPSO();
	render::cleanUpDescHeap();
	buf::cleanUp();
	render::closeCmdQueue();
	shaders::cleanup();
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
		render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue(),
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
	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;

		swapChain->GetBuffer(i, IID_PPV_ARGS(&resource));

		swapchainFB[i] = new framebuffer();

		swapchainFB[i]->addFBOfromBuf(resource);

		//will be changed
		swapchainFB[i]->setDepthClear(1.0f);
	}

	gbufferFB = new framebuffer();
	//position
	gbufferFB->createAddFBO(e_globWindow.width(), e_globWindow.height(), DXGI_FORMAT_R32G32B32A32_FLOAT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//normal + tex
	gbufferFB->createAddFBO(e_globWindow.width(), e_globWindow.height(), DXGI_FORMAT_R32G32B32A32_FLOAT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	gbufferFB->setDepthClear(1.0f);

	return true;
}

void renderer::preDraw(float dt)
{

}

void renderer::draw(float dt)
{
	{
		auto cmdAllocator = render::getCmdQueue(render::QUEUE_GRAPHIC)->getAllocator();
		auto cmdList = render::getCmdQueue(render::QUEUE_GRAPHIC)->getCmdList();

		cmdAllocator->Reset();
		render::getpipelinestate(render::PSO_GBUFFER)->bindPSO(render::getCmdQueue(render::QUEUE_GRAPHIC));

		gbufferFB->openFB(cmdList);

		e_globWorld.drawWorld(cmdList);

		gbufferFB->closeFB(cmdList);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

		render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();
	}

	auto cmdAllocator = render::getCmdQueue(render::QUEUE_GRAPHIC)->getAllocator();
	auto cmdList = render::getCmdQueue(render::QUEUE_GRAPHIC)->getCmdList();

	cmdAllocator->Reset();
	render::getpipelinestate(render::PSO_PBR)->bindPSO(render::getCmdQueue(render::QUEUE_GRAPHIC));

	swapchainFB[frameIndex]->openFB(cmdList);

	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT{ 0.0f, 0.0f, (float)e_globWindow.width(), (float)e_globWindow.height() };
	CD3DX12_RECT scissorRect = CD3DX12_RECT{ 0, 0, (long)e_globWindow.width(), (long)e_globWindow.height() };
	
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
	
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cmdList->IASetVertexBuffers(0, 1, &msh::getMesh(msh::MESH_SCENE_TRIANGLE)->getData()->vbs->view);

	gbufferFB->setgraphicsDescHandle(cmdList, 0, 0);
	gbufferFB->setgraphicsDescHandle(cmdList, 1, 1);
	cmdList->SetGraphicsRootDescriptorTable(2, e_globWorld.getMainCam()->desc.getHandle());

	cmdList->DrawInstanced(3, 1, 0, 0);

	gui::render(cmdList.Get());

	swapchainFB[frameIndex]->closeFB(cmdList);

	render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

	TC_CONDITION(swapChain->Present(vsync, DXGI_PRESENT_ALLOW_TEARING) == S_OK, "Failed to present the swapchain");

	//signal the queue graphics fence and wait for it.
	render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

	frameIndex = swapChain->GetCurrentBackBufferIndex();
}
