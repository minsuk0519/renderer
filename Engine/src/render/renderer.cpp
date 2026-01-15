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

//TODO : Split this
constexpr uint COPYING_GPU_BUFFER_SIZE = 262144 * 8;

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
	descriptor fontDesc = descriptorHeap->requestdescriptor(buf::BUFFER_IMAGE_TYPE, nullptr, nullptr);
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
	TC_INIT(render::initDescHeap());
	e_globBufAllocator.init();
	uploadBuffer = e_globBufAllocator.alloc(nullptr, COPYING_GPU_BUFFER_SIZE, 1, 0, buf::RESOURCE_UPLOAD);
	ubManager = new render::UBManager();
	TC_INIT(ubManager->init());
	TC_INIT(buf::loadResources());
	TC_INIT(createFrameResources());
	TC_INIT(msh::loadResources());

	TC_INIT(initGui());

	for (uint i = 0; i < 3; ++i)
	{
		ssaoTex[i] = e_globBufAllocator.alloc(nullptr, 0, 1, buf::GBF_SRV | buf::GBF_UAV, buf::RESOURCE_TEXTURE, DXGI_FORMAT_R32_FLOAT, e_globWindow.width(), e_globWindow.height());
	}

	commandBuffer = e_globBufAllocator.alloc(nullptr, (MAX_OBJECTS * 2 + 1) * sizeof(uint) * 5, 1, buf::GBF_UAV | buf::GBF_CBV, 0);
	objectConstBuffer = e_globBufAllocator.alloc(nullptr, consts::CONST_OBJ_SIZE * 256, 1, buf::GBF_CBV, 0);
	localClusterOffsetBuffer = e_globBufAllocator.alloc(nullptr, MAX_CLUSTERS * sizeof(uint) * 2, 1, buf::GBF_UAV | buf::GBF_SRV, 0);
	localClusterSizeBuffer = e_globBufAllocator.alloc(nullptr, sizeof(uint) * 12, 1, buf::GBF_UAV, 0);
	clusterArgsBuffer = e_globBufAllocator.alloc(nullptr, (MAX_CLUSTERS / THREADS_NUM_CLUSTERS) * sizeof(uint), 1, buf::GBF_UAV, 0);
	viewInfoBuffer = e_globBufAllocator.alloc(nullptr, MAX_OBJECTS * sizeof(float) * 10, 1, buf::GBF_SRV, buf::RESOURCE_UPLOAD, DXGI_FORMAT_R32_TYPELESS);
	
#if ENGINE_DEBUG_BUFFER
	outDebugBuffer = e_globBufAllocator.alloc(nullptr, 65536 * 1024 * sizeof(uint), 1, buf::GBF_UAV, 0);
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
	uint sWidth = e_globWindow.width();
	uint sHeight = e_globWindow.height();

	{
		fbDepth = e_globBufAllocator.alloc(nullptr, 0, 3, buf::GBF_DEPTH_STENCIL, buf::RESOURCE_DEPTH | buf::RESOURCE_TEXTURE | buf::RESOURCE_CLEAR, DXGI_FORMAT_D32_FLOAT, sWidth, sHeight, 1);
	}

	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;

		swapChain->GetBuffer(i, IID_PPV_ARGS(&resource));

		//will corresponding with createSwapChain()
		swapchainFB[i] = new framebuffer();
		swapchainFB[i]->attachResource(resource.Get(), sWidth, sHeight, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	}

	gbufferFB = new framebuffer();
	//position
	gbufferFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32G32B32A32_FLOAT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//normal
	gbufferFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//objInfo
	gbufferFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	gbufferFB->attachDepth(fbDepth, 1.0f);

#if ENGINE_DEBUG_DEBUGCAM
	//should be sync with gbufferFB
	gbufferDebugFB = new framebuffer();
	//position
	gbufferDebugFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32G32B32A32_FLOAT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//normal
	gbufferDebugFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//objInfo
	gbufferDebugFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	gbufferDebugFB->attachDepth(fbDepth, 1.0f);
#endif // #if ENGINE_DEBUG_DEBUGCAM

	debugFB = new framebuffer();
	debugFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R8_UNORM, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	debugFB->attachDepth(fbDepth, 1.0f);

	{
		std::vector<render::cmdSigData> sigData[2];

		//sigData[0].push_back({ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT, CBV_SCREEN, 1 });

		render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->setCommandSignature(sigData[0]);
		render::getpipelinestate(render::PSO_CULLCLUSTER)->setCommandSignature(sigData[1]);
	}

	return true;
}

void renderer::setVertexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, uint slot, buffer* buf)
{
	D3D12_VERTEX_BUFFER_VIEW view;

	view.BufferLocation = buf->getResource()->GetGPUVirtualAddress();
	view.SizeInBytes = buf->getHeader()->dataSize;
	view.StrideInBytes = buf->getHeader()->packedData.stride * sizeof(float);

	cmdList->IASetVertexBuffers(slot, 1, &view);
}

void renderer::setIndexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, buffer* buf)
{
	D3D12_INDEX_BUFFER_VIEW view;

	view.BufferLocation = buf->getResource()->GetGPUVirtualAddress();
	//todo?
	view.Format = DXGI_FORMAT_R32_UINT;
	view.SizeInBytes = buf->getHeader()->dataSize;

	cmdList->IASetIndexBuffer(&view);
}

void renderer::copyGPUBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, buffer* dst, uint dstOffset, buffer* src, uint srcOffset, uint size)
{
	TC_ASSERT(dst->getHeader()->dataSize >= size);

	D3D12_RESOURCE_STATES srcBarrier = dst->getCurResourceState();

	CD3DX12_RESOURCE_BARRIER barrier;

	//src don't need it because it is D3D12_RESOURCE_STATE_GENERIC_READ
	barrier = dst->getTransition(D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->ResourceBarrier(1, &barrier);

	cmdList->CopyBufferRegion(dst->getResource(), dstOffset, src->getResource(), srcOffset, size);

	barrier = dst->getTransition(srcBarrier);

	cmdList->ResourceBarrier(1, &barrier);
}

void renderer::uploadGPUBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, buffer* dst, uint dstOffset, void* data, uint size)
{
	//todo : change it to srcOffset
	uint offset = 0;

	uploadBuffer->uploadBuffer(size, offset, data);

	copyGPUBuffer(cmdList, dst, dstOffset, uploadBuffer, offset, size);
}

void renderer::uploadCopyGPUBuffer(ID3D12Resource* resource, void* data, uint size)
{
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList = render::getCmdQueue(render::QUEUE_COPY)->getCmdList();

	uint offset = 0;

	uploadBuffer->uploadBuffer(size, offset, data);

	cmdList->Reset(render::getCmdQueue(render::QUEUE_COPY)->getAllocator().Get(), nullptr);

	cmdList->CopyBufferRegion(resource, 0, uploadBuffer->getResource(), offset, size);

	render::getCmdQueue(render::QUEUE_COPY)->execute({ cmdList });
	render::getCmdQueue(render::QUEUE_COPY)->flush();
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

		//msh->setBuffer(cmdList, false);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, (D3D12_GPU_DESCRIPTOR_HANDLE)debugProjection);

		msh->draw(cmdList);

		debugFB->closeFB(cmdList);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

		render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

		//debugFBRequest = false;

		render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->EndEvent();
	}

	render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->BeginEvent(1, "InstanceCull", sizeof("InstanceCull"));

	e_globWorld.instanceCulling();

	render::getCmdQueue(render::QUEUE_GRAPHIC)->getQueue()->EndEvent();
}

void renderer::setUpTerrain()
{
	uint indexSize = 512 * 512 * sizeof(uint) * 3 * 2;
	buffer* terrainVert[3];
	//TODO : arbitrary size
	terrainVert[0] = e_globBufAllocator.alloc(nullptr, 513 * 513 * sizeof(float) * 4 * 3, 3, buf::GBF_SRV | buf::GBF_UAV, buf::RESOURCE_ONETIME);
	terrainVert[1] = e_globBufAllocator.alloc(nullptr, 513 * 513 * sizeof(float) * 4 * 3, 3, buf::GBF_SRV | buf::GBF_UAV, buf::RESOURCE_ONETIME);
	terrainVert[2] = e_globBufAllocator.alloc(nullptr, indexSize, 3, buf::GBF_SRV | buf::GBF_UAV, buf::RESOURCE_ONETIME);

	buffer* noise;

	noise = e_globBufAllocator.alloc(nullptr, 513 * 513 * sizeof(float), 1, buf::GBF_UAV | buf::GBF_SRV, buf::RESOURCE_ONETIME);

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENNOISE);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_NOISE, noise->getDesc(buf::GBF_UAV)->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_NOISECONST, 2, &renderGuiSetting::noiseConstants);

		computeCmdList->Dispatch(513, 513, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENTERRAIN);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TERRAIN_VERT, terrainVert[0]->getDesc(buf::GBF_UAV)->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TERRAIN_NORM, terrainVert[1]->getDesc(buf::GBF_UAV)->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_TERRAIN_NOISE, noise->getDesc(buf::GBF_SRV)->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_TERRAINCONST, 1, &renderGuiSetting::terrainConstants);

		computeCmdList->Dispatch(74, 74, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENTERRAININDEX);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TERRAIN_INDEX, terrainVert[2]->getDesc(buf::GBF_UAV)->getHandle());

		computeCmdList->Dispatch(64, 64, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	meshData* newData = msh::setUpTerrain(indexSize);
	
	e_globRenderer.uploadMeshToUB(terrainVert[0], terrainVert[1], terrainVert[2], newData, msh::MESH_TERRAIN, MESH_INFO_FLAGS_TERRAIN);
}

struct cmdConsts
{
	uint packedID[128 * 4] = { 0 };
	uint objCount = 0;
};

void renderer::setUp()
{
	//render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->BeginEvent(1, "Generate Terrain", sizeof("Generate Terrain"));

	//setUpTerrain();
	//
	//render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->EndEvent();

	{
		//todo stride
		cmdConstBuffer = e_globBufAllocator.alloc(nullptr, 513 * sizeof(uint), 6, buf::GBF_CBV, buf::RESOURCE_UPLOAD);
	}

#if ENGINE_DEBUG_DEBUGCAM
	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->BeginEvent(1, "Setup WireFrame Buffer", sizeof("Setup WireFrame Buffer"));

	{
		float cubeVertices[] =
		{
			1.0f,  1.0f,  1.0f,
			1.0f, -1.0f,  1.0f,
		   -1.0f,  1.0f,  1.0f,
		   -1.0f, -1.0f,  1.0f,

			1.0f,  1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
		   -1.0f,  1.0f, -1.0f,
		   -1.0f, -1.0f, -1.0f,
		};
		AABBwireframeBuffer[0] = e_globBufAllocator.alloc(reinterpret_cast<char*>(cubeVertices), 24 * sizeof(float), 3, buf::GBF_NONE, buf::RESOURCE_NONE);
		AABBwireframeBuffer[1] = e_globBufAllocator.alloc(nullptr, MAX_OBJECTS * 6 * sizeof(float), 6, buf::GBF_NONE, buf::RESOURCE_UPLOAD);
		uint cubeIndicesLine[] = 
		{
			0, 1, 1, 3,
			3, 2, 2, 0,

			4, 5, 5, 7,
			7, 6, 6, 4,

			0, 4, 1, 5,
			2, 6, 3, 7,
		};
		AABBwireframeBuffer[2] = e_globBufAllocator.alloc(reinterpret_cast<char*>(cubeIndicesLine), 24 * sizeof(uint), 2, buf::GBF_NONE, buf::RESOURCE_NONE);
	}

	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->EndEvent();
#endif // #if ENGINE_DEBUG_DEBUGCAM

	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->BeginEvent(1, "Create Triangle Buffer", sizeof("Create Triangle Buffer"));

	{
		//create triangle vertex
        float triangleVertices[] =
        {
            0.0, 0.5, 0.0f,
            0.5, -0.5, 0.0f,
            0.0, -0.5, 0.0f,
        };

		triangleBuffer = e_globBufAllocator.alloc(reinterpret_cast<char*>(triangleVertices), 9 * sizeof(float), 3, buf::GBF_NONE, buf::RESOURCE_NONE);
	}

	{
		//create triangle vertex
		float triangleVertices[] =
		{
			-1.0,  3.0, 0.0f,
			 3.0, -1.0, 0.0f,
			-1.0, -1.0, 0.0f,
		};

		sceneTriangleBuffer = e_globBufAllocator.alloc(reinterpret_cast<char*>(triangleVertices), 9 * sizeof(float), 3, buf::GBF_NONE, buf::RESOURCE_NONE);
	}

	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->EndEvent();
}

void renderer::draw(float dt)
{
	unsigned char* dataPtr = nullptr;
	viewInfoBuffer->mapBuffer(&dataPtr);
	e_globWorld.uploadObjectViewInfo(dataPtr);
	viewInfoBuffer->unmapBuffer();

	descriptor* clustersizeUAV = localClusterSizeBuffer->getDesc(buf::GBF_UAV);
	descriptor* clusteroffsetUAV = localClusterOffsetBuffer->getDesc(buf::GBF_UAV);
	descriptor* clusteroffsetSRV = localClusterOffsetBuffer->getDesc(buf::GBF_SRV);
	descriptor* meshInfoBufferSRV = ubManager->meshInfoBuffer->getDesc(buf::GBF_SRV);
	descriptor* lodInfoBufferSRV = ubManager->lodInfoBuffer->getDesc(buf::GBF_SRV);
	descriptor* clusterInfoBufferSRV = ubManager->clusterInfoBuffer->getDesc(buf::GBF_SRV);
	descriptor* vertexIDBufferSRV = ubManager->vertexIDBuffer->getDesc(buf::GBF_SRV);
	descriptor* vertexIDBufferUAV = ubManager->vertexIDBuffer->getDesc(buf::GBF_UAV);
	descriptor* clusterBoundBufferSRV = ubManager->clusterBoundBuffer->getDesc(buf::GBF_SRV);
	descriptor* viewInfoBufferSRV = viewInfoBuffer->getDesc(buf::GBF_SRV);
#if	ENGINE_DEBUG_BUFFER
	descriptor* outDebugBufferUAV = outDebugBuffer->getDesc(buf::GBF_UAV);
#endif // #if ENGINE_DEBUG_BUFFER
	descriptor* unifiedVertexBufferUAV = ubManager->unifiedVertexBuffer->getDesc(buf::GBF_SRV);
	descriptor* unifiedIndexBufferUAV = ubManager->unifiedIndexBuffer->getDesc(buf::GBF_SRV);
	descriptor* camDesc = e_globWorld.getMainCam()->getDesc();

	descriptor* ssaoUAV[3];
	descriptor* ssaoSRV[3];
	ssaoUAV[0] = ssaoTex[0]->getDesc(buf::GBF_UAV);
	ssaoUAV[1] = ssaoTex[1]->getDesc(buf::GBF_UAV);
	ssaoUAV[2] = ssaoTex[2]->getDesc(buf::GBF_UAV);
	ssaoSRV[0] = ssaoTex[0]->getDesc(buf::GBF_SRV);
	ssaoSRV[1] = ssaoTex[1]->getDesc(buf::GBF_SRV);
	ssaoSRV[2] = ssaoTex[2]->getDesc(buf::GBF_SRV);

	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->BeginEvent(1, "InitCluster", sizeof("InitCluster"));

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_INITCLUSTER);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());

		computeCmdList->Dispatch(1, 1, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}
	
	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->EndEvent();

	render::getCmdQueue(render::QUEUE_COMPUTE)->getQueue()->BeginEvent(1, "Triangle Processing", sizeof("Triangle Processing"));

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_UPLOADLOCALOBJ);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTEROFFSET_BUFFER, clusteroffsetUAV->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());

		unsigned char* cbvDataBegin;
		cmdConstBuffer->mapBuffer(&cbvDataBegin);
		uint objCount = e_globWorld.submitObjects(cbvDataBegin);

		memcpy(cbvDataBegin + 64 * 4 * 4, &objCount, 4);

		cmdConstBuffer->unmapBuffer();

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_CMDBUFCONSTS, cmdConstBuffer->getDesc(buf::GBF_CBV)->getHandle());

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_MESHINFO_BUFFER, meshInfoBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LOD_INFO_BUFFER, lodInfoBufferSRV->getHandle());

		computeCmdList->Dispatch(objCount, 1, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_CULLCLUSTER);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LOD_INFO_BUFFER, lodInfoBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_INFO_BUFFER, clusterInfoBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_ARGS_BUFFER, clusteroffsetSRV->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERARGS_BUFFER, vertexIDBufferUAV->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_BOUNDS_BUFFER, clusterBoundBufferSRV->getHandle());

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_VIEWINFO_BUFFER, viewInfoBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, camDesc->getHandle());

#if ENGINE_DEBUG_BUFFER
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_GLOBAL_DEBUG_BUFFER, outDebugBufferUAV->getHandle());
#endif // #if ENGINE_DEBUG_BUFFER

		computeCmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_CULLCLUSTER)->getCmdSignature(), 1, localClusterSizeBuffer->getResource(), sizeof(uint), nullptr, 0);

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

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VERTEX_BUFFER, unifiedVertexBufferUAV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_INDEX_BUFFER, unifiedIndexBufferUAV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_CLUSTERARGS, vertexIDBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VIEWINFO_BUFFER, viewInfoBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_MESHINFO_BUFFER, meshInfoBufferSRV->getHandle());

		cmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->getCmdSignature(), 1, localClusterSizeBuffer->getResource(), 5 * sizeof(uint), nullptr, 0);

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

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VERTEX_BUFFER, unifiedVertexBufferUAV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_INDEX_BUFFER, unifiedIndexBufferUAV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_CLUSTERARGS, vertexIDBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VIEWINFO_BUFFER, viewInfoBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_MESHINFO_BUFFER, meshInfoBufferSRV->getHandle());

		cmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->getCmdSignature(), 1, localClusterSizeBuffer->getResource(), 5 * sizeof(uint), nullptr, 0);
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
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, camDesc->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAO, ssaoUAV[0]->getHandle());
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
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, camDesc->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAO, ssaoUAV[i]->getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAOBLUR, ssaoUAV[i + 1]->getHandle());
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

		setVertexBuffer(cmdList, 0, sceneTriangleBuffer);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_POSITION, gbufferDebugFB->getDescHandle(0));
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_NORM, gbufferDebugFB->getDescHandle(1));
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_DEBUG, gbufferDebugFB->getDescHandle(2));
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, camDesc->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_AO, ssaoSRV[2]->getHandle());
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

	setVertexBuffer(cmdList, 0, sceneTriangleBuffer);

	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_POSITION, gbufferFB->getDescHandle(0));
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_NORM, gbufferFB->getDescHandle(1));
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_DEBUG, gbufferFB->getDescHandle(2));
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, camDesc->getHandle());
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_AO, ssaoSRV[2]->getHandle());
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
		AABBwireframeBuffer[1]->mapBuffer(&aabbGPUAddress);

		e_globWorld.boundData(aabbGPUAddress);

		AABBwireframeBuffer[1]->unmapBuffer();

		render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_AABBDEBUGDRAW);

		swapchainFB[frameIndex]->openFB(cmdList, false);

		e_globWorld.setupCam(cmdList, true, false);

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

		setVertexBuffer(cmdList, 0, AABBwireframeBuffer[0]);
		setVertexBuffer(cmdList, 1, AABBwireframeBuffer[1]);
		setIndexBuffer(cmdList, AABBwireframeBuffer[2]);

		cmdList->DrawIndexedInstanced(24, e_globWorld.objectNum, 0, 0, 0);

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

void renderer::uploadMeshToUB(buffer* vertex, buffer* norm, buffer* index, meshData* meshdata, uint meshID, uint flags)
{
	ubManager->uploadMeshToUB(vertex, norm, index, meshdata, meshID, flags);
}