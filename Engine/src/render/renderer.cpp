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
#include <render/shader_defines.hpp>

#include <system/logger.hpp>
#include <system/window.hpp>
#include <system/gui.hpp>

#include <d3dx12.h>
#include <DirectXMath.h>

//TODO
uint vsync = 0;

uint frameIndex = 0;

renderer e_globRenderer;
renderBuf e_globGPUBuffer;

namespace renderGuiSetting
{
	struct AOConstants
	{
		float s = 1.0f;
		float k = 1.0f;
		float R = 0.5f;
		int num = 10;
	};

	struct NoiseConstants
	{
		uint octaves = 2;
		float zConsts = 1.0f;
	};

	struct guiSetting
	{
		uint features;
		uint debugDraw;
		bool AABBDraw = false;
	};

	guiSetting guiDebug;
	AOConstants aoConstants;
	NoiseConstants noiseConstants;
	float terrainConstants = 500.0f;

	bool ssaoEnabled = true;
}

bool initGui()
{
	//setting up gui
	descriptorheap* descriptorHeap = render::getHeap(render::DESCRIPTORHEAP_BUFFER);
	//buffer type does not matter
	descriptor fontDesc = descriptorHeap->requestdescriptor(buf::BUFFER_IMAGE_TYPE, nullptr);
	gui::init(e_globWindow.getWindow(), e_globRenderer.device.Get(), descriptorHeap->getHeap(), fontDesc);

	return true;
}

bool renderer::init(Microsoft::WRL::ComPtr<IDXGIFactory4> dxFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter)
{
	factory = dxFactory;

	TC_CONDITIONB(createDevice(factory, adapter) == true, "Failed to create device");
	TC_INIT(shaders::loadResources());
	TC_INIT(render::initPSO());
	TC_INIT(render::allocateCmdQueue());
	TC_CONDITIONB(createSwapChain() == true, "Failed to create swapchain");
	TC_INIT(buf::loadResources());
	TC_INIT(render::initDescHeap());
	TC_INIT(createUB());
	TC_INIT(createFrameResources());
	TC_INIT(msh::loadResources());

	TC_INIT(initGui());

	for (uint i = 0; i < 3; ++i)
	{
		ssaoTex[i] = buf::createImageBuffer(e_globWindow.width(), e_globWindow.height(), 1, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		ssaoDesc[i] = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, ssaoTex[i]);
	}

	terrainTex[0] = buf::createUAVBuffer(513 * 513 * sizeof(float) * 4 * 3);
	terrainTex[1] = buf::createUAVBuffer(513 * 513 * sizeof(float) * 4 * 3);
	terrainTex[2] = buf::createUAVBuffer(512 * 512 * sizeof(uint) * 3 * 2);
	for (uint i = 0; i < 3; ++i)
	{
		terrainDesc[i] = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_UAV_TYPE, terrainTex[i]);
	}

//TODO : arbitrary size
	//uvb
	unifiedBuffer[0] = buf::createUAVBuffer(16777216 * 3 * 2 * sizeof(float));
	//uib
	unifiedBuffer[1] = buf::createUAVBuffer(16777216 * (sizeof(uint) * 3 + 3));
	//clusterIDs
	vertexIDBufferUAV = buf::createUAVBuffer(16777216 * sizeof(uint) * 3 * 2);
	for (uint i = 0; i < 2; ++i)
	{
		unifiedDesc[i] = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_UAV_TYPE, unifiedBuffer[i]);
	}
	vertexIDBuffer = buf::createVertexBufferFromUAV(vertexIDBufferUAV, sizeof(uint));
	vertexIDDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_UAV_TYPE, vertexIDBufferUAV);

	commandBuffer = buf::createUAVBuffer((MAX_OBJECTS * 2 + 1) * sizeof(uint) * 5);
	commandDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_UAV_TYPE, commandBuffer);

	objectConstBuffer = buf::createConstantBuffer(consts::CONST_OBJ_SIZE * 256);
	objectConstDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_CONSTANT_TYPE, objectConstBuffer);

	localClusterOffsetBuffer = buf::createUAVBuffer(MAX_CLUSTERS * sizeof(uint) * 2);
	localClusterOffsetDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_UAV_TYPE, localClusterOffsetBuffer);
	localClusterSizeBuffer = buf::createUAVBuffer(sizeof(uint) * 12);
	localClusterSizeDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_UAV_TYPE, localClusterSizeBuffer);
	clusterArgsBuffer = buf::createUAVBuffer((MAX_CLUSTERS / THREADS_NUM_CLUSTERS) * sizeof(uint));
	clusterArgsDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_UAV_TYPE, clusterArgsBuffer);
	viewInfoBuffer = buf::createImageBuffer(MAX_OBJECTS * sizeof(float) * 10, 0, 0, DXGI_FORMAT_R32_TYPELESS);
	viewInfoDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, viewInfoBuffer);

#if ENGINE_DEBUG_BUFFER
	outDebugBuffer = buf::createUAVBuffer(65536 * 1024 * sizeof(uint));
	outDebugDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_UAV_TYPE, outDebugBuffer);
#endif // #if ENGINE_DEBUG_BUFFER
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

	TC_CONDITIONB(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)) == S_OK, "Failed to create device");

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
	}

	gbufferFB = new framebuffer();
	//position
	gbufferFB->createAddFBO(e_globWindow.width(), e_globWindow.height(), DXGI_FORMAT_R32G32B32A32_FLOAT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//normal
	gbufferFB->createAddFBO(e_globWindow.width(), e_globWindow.height(), DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//objInfo
	gbufferFB->createAddFBO(e_globWindow.width(), e_globWindow.height(), DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	gbufferFB->setDepthClear(1.0f);

#if ENGINE_DEBUG_DEBUGCAM
	//should be sync with gbufferFB
	gbufferDebugFB = new framebuffer();
	//position
	gbufferDebugFB->createAddFBO(e_globWindow.width(), e_globWindow.height(), DXGI_FORMAT_R32G32B32A32_FLOAT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//normal
	gbufferDebugFB->createAddFBO(e_globWindow.width(), e_globWindow.height(), DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//objInfo
	gbufferDebugFB->createAddFBO(e_globWindow.width(), e_globWindow.height(), DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	gbufferDebugFB->setDepthClear(1.0f);
#endif // #if ENGINE_DEBUG_DEBUGCAM

	debugFB = new framebuffer();
	debugFB->createAddFBO(e_globWindow.width(), e_globWindow.height(), DXGI_FORMAT_R8_UNORM, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	debugFB->setDepthClear(1.0f);

	{
		std::vector<render::cmdSigData> sigData[2];

		//sigData[0].push_back({ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT, CBV_SCREEN, 1 });

		render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->setCommandSignature(sigData[0]);
		render::getpipelinestate(render::PSO_CULLCLUSTER)->setCommandSignature(sigData[1]);
	}

	return true;
}

framebuffer* renderer::getFrameBuffer() const
{
	return gbufferFB;
}

framebuffer* renderer::getDebugFrameBuffer() const
{
	return debugFB;
}

void renderer::debugFrameBufferRequest(uint debugMeshID, UINT64 ptr)
{
	debugFBRequest = true;
	debugFBMeshID = debugMeshID;
	debugProjection = ptr;
}

const char* debugDrawVersion[]
{
	"None",
	"Position",
	"Normal",
	"Debug",
	"SSAO",
};

void renderer::guiSetting()
{
	gui::comboBox("DebugDraw", debugDrawVersion, 5, renderGuiSetting::guiDebug.debugDraw);

	ImGui::Checkbox("SSAO", &renderGuiSetting::ssaoEnabled);
	ImGui::Checkbox("ShowAABB", &renderGuiSetting::guiDebug.AABBDraw);

	if(renderGuiSetting::ssaoEnabled) renderGuiSetting::guiDebug.features |= FEATURE_AO;
	else renderGuiSetting::guiDebug.features &= ~FEATURE_AO;

	if (renderGuiSetting::ssaoEnabled)
	{
		if (ImGui::CollapsingHeader("SSAO"))
		{
			ImGui::DragFloat("Scale Value##SSAO", &renderGuiSetting::aoConstants.s, 0.1f, 0.0f, 1.0f);
			ImGui::DragFloat("Scale Value2##SSAO", &renderGuiSetting::aoConstants.k, 1.0f, 0.0f, 5.0f);
			ImGui::DragFloat("Radius##SSAO", &renderGuiSetting::aoConstants.R, 0.1f, 0.0f, 5.0f);
			ImGui::DragInt("Num##SSAO", &renderGuiSetting::aoConstants.num, 1.0f, 1, 100);
		}
	}
}



bool renderer::createUB()
{
	meshInfoBuffer->uploadBuffer(meshInfoSize, 0, meshInfos);
	lodInfoBuffer->uploadBuffer(lodInfoSize, 0, lodInfos);
	clusterInfoBuffer->uploadBuffer(clusterInfoSize, 0, clusterInfos);
	clusterBoundBuffer->uploadBuffer(curClusterOffset * sizeof(clusterbounddata), 0, clusterBounds);

	//uvb
	unifiedBuffer[0] = buf::createUAVBuffer(16777216 * 3 * 2 * sizeof(float));
	//uib
	unifiedBuffer[1] = buf::createUAVBuffer(16777216 * (sizeof(uint) * 3 + 3));
	//clusterIDs
	vertexIDBufferUAV = buf::createUAVBuffer(16777216 * sizeof(uint) * 3 * 2);

	return false;
}

void renderer::preDraw(float dt)
{
	if (debugFBRequest)
	{
		render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->BeginEvent(1, "DebugMeshDraw", sizeof("DebugMeshDraw"));

		auto cmdList = render::getCmdQueue(render::QUEUE_GRAPHIC)->getCmdList();
		mesh* msh = msh::getMesh((msh::MESH_INDEX)debugFBMeshID);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_WIREFRAME);

		debugFB->openFB(cmdList, true);

		CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT{ 0.0f, 0.0f, (float)e_globWindow.width(), (float)e_globWindow.height() };
		CD3DX12_RECT scissorRect = CD3DX12_RECT{ 0, 0, (long)e_globWindow.width(), (long)e_globWindow.height() };

		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		msh->setBuffer(cmdList, false);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, (D3D12_GPU_DESCRIPTOR_HANDLE)debugProjection);

		msh->draw(cmdList);

		debugFB->closeFB(cmdList);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

		render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

		//debugFBRequest = false;

		render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->EndEvent();
	}

	e_globWorld.instanceCulling();
}

void renderer::setUpTerrain()
{
	imagebuffer* noiseTex;
	descriptor noiseDesc;
	uavbuffer* noiseUAV;

	noiseUAV = buf::createUAVBuffer(513 * 513 * sizeof(float));
	noiseTex = buf::createImageBufferFromBuffer(noiseUAV, { 0, 513 * 513, sizeof(float), D3D12_BUFFER_SRV_FLAG_NONE });
	noiseDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, noiseTex);

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENNOISE);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_NOISE, noiseDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_NOISECONST, 2, &renderGuiSetting::noiseConstants);

		computeCmdList->Dispatch(513, 513, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENTERRAIN);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TERRAIN_VERT, terrainDesc[0].getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TERRAIN_NORM, terrainDesc[1].getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_TERRAIN_NOISE, noiseDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_TERRAINCONST, 1, &renderGuiSetting::terrainConstants);

		computeCmdList->Dispatch(74, 74, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENTERRAININDEX);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TERRAIN_INDEX, terrainDesc[2].getHandle());

		computeCmdList->Dispatch(64, 64, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	vertexbuffer* v = buf::createVertexBufferFromUAV(terrainTex[0], sizeof(float) * 3);
	vertexbuffer* n = buf::createVertexBufferFromUAV(terrainTex[1], sizeof(float) * 3);
	indexbuffer* i = buf::createIndexBufferFromUAV(terrainTex[2]);
	msh::setUpTerrain(v, n, i);
}

struct cmdConsts
{
	uint packedID[128 * 4] = { 0 };
	uint objCount = 0;
};

void renderer::setUp()
{
	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->BeginEvent(1, "Generate Terrain", sizeof("Generate Terrain"));

	setUpTerrain();
	
	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->EndEvent();

	{
		cmdConstBuffer = buf::createConstantBuffer(513 * sizeof(uint));
		commandConstDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_CONSTANT_TYPE, cmdConstBuffer);
	}

	{
		AABBwireframeBuffer = buf::createVertexBuffer(nullptr, 24 * sizeof(float), 6 * sizeof(float));
	}
}

void renderer::draw(float dt)
{
	unsigned char* dataPtr = nullptr;
	viewInfoBuffer->mapBuffer(&dataPtr);
	e_globWorld.uploadObjectViewInfo(dataPtr);
	viewInfoBuffer->unmapBuffer();

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_INITCLUSTER);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, localClusterSizeDesc.getHandle());

		computeCmdList->Dispatch(1, 1, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}
	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->BeginEvent(1, "Triangle Processing", sizeof("Triangle Processing"));

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_UPLOADLOCALOBJ);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTEROFFSET_BUFFER, localClusterOffsetDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, localClusterSizeDesc.getHandle());

		uint objCount = e_globWorld.submitObjects(cmdConstBuffer->info.cbvDataBegin);

		memcpy(cmdConstBuffer->info.cbvDataBegin + 64 * 4 * 4, &objCount, 4);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_CULLINGCONSTS, commandConstDesc.getHandle());

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_MESH_INFO_BUFFER, meshInfoDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LOD_INFO_BUFFER, lodInfoDesc.getHandle());

		computeCmdList->Dispatch(objCount, 1, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_CULLCLUSTER);

		//render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_MESH_INFO_BUFFER, meshInfoDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LOD_INFO_BUFFER, lodInfoDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_INFO_BUFFER, clusterInfoDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_ARGS_BUFFER, localClusterOffsetDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERARGS_BUFFER, vertexIDDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, localClusterSizeDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_BOUNDS_BUFFER, clusterBoundDesc.getHandle());

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_VIEW_INFOS_BUFFER, viewInfoDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_CULLING_PROJECTION, e_globWorld.getMainCam()->desc.getHandle());

#if ENGINE_DEBUG_BUFFER
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_GLOBAL_DEBUG_BUFFER, outDebugDesc.getHandle());
#endif // #if ENGINE_DEBUG_BUFFER

		computeCmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_CULLCLUSTER)->getCmdSignature(), 1, localClusterSizeBuffer->resource.Get(), sizeof(uint), nullptr, 0);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->EndEvent();

#if ENGINE_DEBUG_DEBUGCAM
	if (e_globWorld.getMainCam()->viewportType != cam::VIEWPORT_FULL)
	{
		auto cmdList = render::getCmdQueue(render::QUEUE_GRAPHIC)->getCmdList();

		render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_GBUFFERINDIRECT);

		gbufferDebugFB->openFB(cmdList, true);

		e_globWorld.setupCam(cmdList, false, true);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->BeginEvent(1, "Draw GBuffer", sizeof("Draw GBuffer"));

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_VERTEX, unifiedDesc[0].getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_NORM, unifiedDesc[1].getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_CLUSTERARGS, vertexIDDesc.getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_VIEWINFO, viewInfoDesc.getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_MESHINFO, meshInfoDesc.getHandle());

		cmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->getCmdSignature(), 1, localClusterSizeBuffer->resource.Get(), 5 * sizeof(uint), nullptr, 0);

		gbufferDebugFB->closeFB(cmdList);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

		render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

		render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->EndEvent();
	}
#endif // #if ENGINE_DEBUG_DEBUGCAM

	auto cmdList = render::getCmdQueue(render::QUEUE_GRAPHIC)->getCmdList();

	render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_GBUFFERINDIRECT);

	gbufferFB->openFB(cmdList, true);

	e_globWorld.setupCam(cmdList, true, true);

	{
		render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->BeginEvent(1, "Draw GBuffer", sizeof("Draw GBuffer"));

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_VERTEX, unifiedDesc[0].getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_NORM, unifiedDesc[1].getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_CLUSTERARGS, vertexIDDesc.getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_VIEWINFO, viewInfoDesc.getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_MESHINFO, meshInfoDesc.getHandle());

		cmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->getCmdSignature(), 1, localClusterSizeBuffer->resource.Get(), 5 * sizeof(uint), nullptr, 0);
	}

	gbufferFB->closeFB(cmdList);

	render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

	render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

	render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->EndEvent();

	render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->BeginEvent(1, "Draw SSAO", sizeof("Draw SSAO"));

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_SSAO);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LIGHT_POSITION, gbufferFB->getDescHandle(0));
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LIGHT_NORM, gbufferFB->getDescHandle(1));
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, e_globWorld.getMainCam()->desc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAO, ssaoDesc[0].getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_AOCONST, 4, &renderGuiSetting::aoConstants);
		uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_SCREEN, 2, screenSize);

		computeCmdList->Dispatch(e_globWindow.width() / 8, e_globWindow.height() / 8, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->BeginEvent(1, "Blur AO", sizeof("Blur AO"));

	for(uint i = 0; i < 2; ++i)
	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO((render::PSO_INDEX)((uint)render::PSO_SSAOBLURX + i));

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LIGHT_POSITION, gbufferFB->getDescHandle(0));
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LIGHT_NORM, gbufferFB->getDescHandle(1));
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, e_globWorld.getMainCam()->desc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAO, ssaoDesc[i].getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAOBLUR, ssaoDesc[i + 1].getHandle());
		uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_SCREEN, 2, screenSize);

		computeCmdList->Dispatch(e_globWindow.width() / 8, e_globWindow.height() / 8, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->EndEvent();
	render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->EndEvent();

	render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_PBR);

	render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->BeginEvent(1, "Draw light", sizeof("Draw light"));

	swapchainFB[frameIndex]->openFB(cmdList, true);

#if ENGINE_DEBUG_DEBUGCAM
	if (e_globWorld.getMainCam()->viewportType != cam::VIEWPORT_FULL)
	{
		e_globWorld.setupCam(cmdList, false, false);

		cmdList->IASetVertexBuffers(0, 1, &msh::getMesh(msh::MESH_SCENE_TRIANGLE)->getData()->vbs->view);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_POSITION, gbufferDebugFB->getDescHandle(0));
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_NORM, gbufferDebugFB->getDescHandle(1));
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_DEBUG, gbufferDebugFB->getDescHandle(2));
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, e_globWorld.getMainCam()->desc.getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_AO, ssaoDesc[2].getHandle());
		renderGuiSetting::guiSetting a;
		a.features = 0;
		a.debugDraw = false;
		a.AABBDraw = false;
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_GUIDEBUG, 2, &a);
		uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_SCREEN, 2, screenSize);

		cmdList->DrawInstanced(3, 1, 0, 0);
	}
#endif // #if ENGINE_DEBUG_DEBUGCAM

	e_globWorld.setupCam(cmdList, true, false);

	cmdList->IASetVertexBuffers(0, 1, &msh::getMesh(msh::MESH_SCENE_TRIANGLE)->getData()->vbs->view);

	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_POSITION, gbufferFB->getDescHandle(0));
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_NORM, gbufferFB->getDescHandle(1));
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_DEBUG, gbufferFB->getDescHandle(2));
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, e_globWorld.getMainCam()->desc.getHandle());
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_AO, ssaoDesc[2].getHandle());
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_GUIDEBUG, 2, &renderGuiSetting::guiDebug);
	uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_SCREEN, 2, screenSize);

	cmdList->DrawInstanced(3, 1, 0, 0);

	gui::render(cmdList.Get());

	swapchainFB[frameIndex]->closeFB(cmdList);
	
	render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

	render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

	render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->EndEvent();

	if(renderGuiSetting::guiDebug.AABBDraw)
	{
		render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->BeginEvent(1, "Draw AABB", sizeof("Draw AABB"));
		unsigned char* aabbGPUAddress = nullptr;
		AABBwireframeBuffer->mapBuffer(&aabbGPUAddress);

		e_globWorld.boundData(aabbGPUAddress);

		AABBwireframeBuffer->unmapBuffer();

		render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_AABBDEBUGDRAW);

		swapchainFB[frameIndex]->openFB(cmdList, false);

		e_globWorld.setupCam(cmdList, true, false);

		cmdList->IASetVertexBuffers(0, 1, &msh::getMesh(msh::MESH_CUBE)->getData()->vbs->view);
		cmdList->IASetVertexBuffers(1, 1, &AABBwireframeBuffer->view);
		cmdList->IASetIndexBuffer(&msh::getMesh(msh::MESH_CUBE)->getData()->idxLine->view);

		cmdList->DrawIndexedInstanced(48, e_globWorld.objectNum, 0, 0, 0);

		swapchainFB[frameIndex]->closeFB(cmdList);
		render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

		render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->EndEvent();
	}

	TC_CONDITION(swapChain->Present(vsync, 0) == S_OK, "Failed to present the swapchain");

	//signal the queue graphics fence and wait for it.
	render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

	frameIndex = swapChain->GetCurrentBackBufferIndex();
}

void renderer::drawWorld(float dt)
{
}
