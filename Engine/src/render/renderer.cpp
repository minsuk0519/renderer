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
	TC_INIT(createFrameResources());
	TC_INIT(msh::loadResources());

	TC_INIT(initGui());

	for (uint i = 0; i < 3; ++i)
	{
		ssaoTex[i] = buf::createImageBuffer(e_globWindow.width(), e_globWindow.height(), 1, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		ssaoDesc[i] = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, ssaoTex[i]);
	}

	terrainTex[0] = buf::createUAVBuffer(512 * 512 * sizeof(float) * 4 * 3);
	terrainTex[1] = buf::createUAVBuffer(512 * 512 * sizeof(float) * 4 * 3);
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
	gbufferFB->setDepthClear(1.0f);

	debugFB = new framebuffer();
	debugFB->createAddFBO(e_globWindow.width(), e_globWindow.height(), DXGI_FORMAT_R8_UNORM, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	debugFB->setDepthClear(1.0f);

	{
		render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->setCommandSignature();
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
	"SSAO",
};

void renderer::guiSetting()
{
	gui::comboBox("DebugDraw", debugDrawVersion, 4, renderGuiSetting::guiDebug.debugDraw);

	ImGui::Checkbox("SSAO", &renderGuiSetting::ssaoEnabled);

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

		debugFBRequest = false;
	}
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
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TERRAIN_INDEX, terrainDesc[2].getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_TERRAIN_NOISE, noiseDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_TERRAINCONST, 1, &renderGuiSetting::terrainConstants);

		computeCmdList->Dispatch(512 / 8, 512 / 8, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	vertexbuffer* v = buf::createVertexBufferFromUAV(terrainTex[0], 12);
	vertexbuffer* n = buf::createVertexBufferFromUAV(terrainTex[1], 12);
	indexbuffer* i = buf::createIndexBufferFromUAV(terrainTex[2]);
	msh::setUpTerrain(v, n, i);
}

struct unifiedConsts
{
	uint clusterOffset = 0;
	uint vertexOffset = 0;
	uint indexOffset = 0;
	uint meshID = 0;
};

struct cmdConsts
{
	uint packedID[128 * 4] = { 0 };
	uint objCount = 0;
};

void renderer::setUp()
{
	setUpTerrain();

	uint clusterOffset = 0;
	uint vertexOffset = 0;
	uint indexOffset = 0;

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENUNIFIED);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_UNIFIED_VERTEX_BUFFER, unifiedDesc[0].getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_UNIFIED_INDDEX_BUFFER, unifiedDesc[1].getHandle());

		D3D12_BUFFER_SRV vertexDesc = {};
		vertexDesc.NumElements = 512 * 512 * 4;
		vertexDesc.StructureByteStride = sizeof(float) * 3;
		D3D12_BUFFER_SRV indexDesc = {};
		indexDesc.NumElements = 512 * 512 * 2;
		indexDesc.StructureByteStride = sizeof(uint) * 3;

		imagebuffer* terrainVertBuffer = buf::createImageBufferFromBuffer(terrainTex[0], vertexDesc);
		imagebuffer* terrainIdxBuffer = buf::createImageBufferFromBuffer(terrainTex[2], indexDesc);

		descriptor vertexSRV = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, terrainVertBuffer);
		descriptor indexSRV = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, terrainIdxBuffer);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_VERTEX_BUFFER, vertexSRV.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_INDEX_BUFFER, indexSRV.getHandle());

		unifiedConsts unifiedconst;

		unifiedconst.meshID = msh::MESH_TERRAIN;

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_UNIFIEDCONSTS, 4, &unifiedconst);

		vertexOffset += 512 * 512 * 3 * 4;
		uint indexSize = 512 * 512 * 3 * 2;
		indexOffset += indexSize;
		clusterOffset += (indexSize / (3 * 64)) + 1;

		computeCmdList->Dispatch(1, 1, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}
	
	for(uint i = 0; i < msh::MESH_END; ++i)
	{
		if (i == msh::MESH_TERRAIN)
		{
			continue;
		}

		meshData* meshdata = msh::getMesh(msh::MESH_INDEX(i))->getData();

		D3D12_BUFFER_SRV vertexDesc = {};
		vertexDesc.NumElements = meshdata->vbs->view.SizeInBytes / (sizeof(float) * 3);
		vertexDesc.StructureByteStride = sizeof(float) * 3;
		D3D12_BUFFER_SRV indexDesc = {};
		indexDesc.NumElements = meshdata->idx->view.SizeInBytes / (sizeof(uint) * 3);
		indexDesc.StructureByteStride = sizeof(uint) * 3;
		
		imagebuffer* vbsBuffer = buf::createImageBufferFromBuffer(meshdata->vbs, vertexDesc);
		imagebuffer* idxBuffer = buf::createImageBufferFromBuffer(meshdata->idx, indexDesc);

		descriptor vertexSRV =  render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, vbsBuffer);
		descriptor indexSRV =  render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, idxBuffer);

		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENUNIFIED);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_UNIFIED_VERTEX_BUFFER, unifiedDesc[0].getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_UNIFIED_INDDEX_BUFFER, unifiedDesc[1].getHandle());

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_VERTEX_BUFFER, vertexSRV.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_INDEX_BUFFER, indexSRV.getHandle());

		unifiedConsts unifiedconst;

		unifiedconst.meshID = i;
		unifiedconst.clusterOffset = clusterOffset;
		unifiedconst.vertexOffset = vertexOffset;
		unifiedconst.indexOffset = indexOffset;

		vertexOffset += meshdata->vbs->view.SizeInBytes / meshdata->vbs->view.StrideInBytes;
		uint indexSize = meshdata->idx->view.SizeInBytes / sizeof(uint);
		indexOffset += indexSize;
		clusterOffset += (indexSize / (3 * 64)) + 1;

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_UNIFIEDCONSTS, 4, &unifiedconst);

		computeCmdList->Dispatch(1, 1, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		cmdConstBuffer = buf::createConstantBuffer(513 * sizeof(uint));
		commandConstDesc = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_CONSTANT_TYPE, cmdConstBuffer);
	}

	{
		uint objIDs[4] = { 0, 1, 2, 3 };

		AABBwireframeBuffer = buf::createVertexBuffer(objIDs, 4 * sizeof(uint), sizeof(uint));
	}
}

#include <world/object.hpp>

void renderer::draw(float dt)
{
	auto cmdList = render::getCmdQueue(render::QUEUE_GRAPHIC)->getCmdList();

	//render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_GBUFFER);
	render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_GBUFFERINDIRECT);

	gbufferFB->openFB(cmdList, true);

	e_globWorld.drawWorld(cmdList, false);

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENCMDBUF);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CMD_BUFFER, commandDesc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CMD_VERTEXID_BUFFER, vertexIDDesc.getHandle());

		uint objCount = e_globWorld.drawObject(cmdConstBuffer->info.cbvDataBegin);

		memcpy(cmdConstBuffer->info.cbvDataBegin + 128 * 4 * 4, &objCount, 4);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_CMDBUFCONSTS, commandConstDesc.getHandle());

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CMD_VERTEX_BUFFER, unifiedDesc[1].getHandle());

		computeCmdList->Dispatch(objCount, 1, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();

		cmdList->IASetVertexBuffers(0, 1, &vertexIDBuffer->view);

		for (uint i = 0; i < e_globWorld.objectNum; ++i)
		{
			e_globWorld.objects[i].sendMat(objectConstBuffer->info.cbvDataBegin);
		}

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_OBJECT, objectConstDesc.getHandle());

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER0_TEX, unifiedDesc[0].getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER1_TEX, unifiedDesc[1].getHandle());

		cmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->getCmdSignature(), objCount, commandBuffer->resource.Get(), 0, commandBuffer->resource.Get(), 5 * MAX_OBJECTS * 2 * sizeof(uint));
	}

	gbufferFB->closeFB(cmdList);

	render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

	render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_SSAO);

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_GBUFFER0_TEX, gbufferFB->getDescHandle(0));
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_GBUFFER1_TEX, gbufferFB->getDescHandle(1));
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, e_globWorld.getMainCam()->desc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAO, ssaoDesc[0].getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_AOCONST, 4, &renderGuiSetting::aoConstants);
		uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_SCREEN, 2, screenSize);

		computeCmdList->Dispatch(e_globWindow.width() / 8, e_globWindow.height() / 8, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	for(uint i = 0; i < 2; ++i)
	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO((render::PSO_INDEX)((uint)render::PSO_SSAOBLURX + i));

		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_GBUFFER0_TEX, gbufferFB->getDescHandle(0));
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_GBUFFER1_TEX, gbufferFB->getDescHandle(1));
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, e_globWorld.getMainCam()->desc.getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAO, ssaoDesc[i].getHandle());
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAOBLUR, ssaoDesc[i + 1].getHandle());
		uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
		render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_SCREEN, 2, screenSize);

		computeCmdList->Dispatch(e_globWindow.width() / 8, e_globWindow.height() / 8, 1);

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_PBR);

	swapchainFB[frameIndex]->openFB(cmdList, true);

	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT{ 0.0f, 0.0f, (float)e_globWindow.width(), (float)e_globWindow.height() };
	CD3DX12_RECT scissorRect = CD3DX12_RECT{ 0, 0, (long)e_globWindow.width(), (long)e_globWindow.height() };

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cmdList->IASetVertexBuffers(0, 1, &msh::getMesh(msh::MESH_SCENE_TRIANGLE)->getData()->vbs->view);

	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER0_TEX, gbufferFB->getDescHandle(0));
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER1_TEX, gbufferFB->getDescHandle(1));
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, e_globWorld.getMainCam()->desc.getHandle());
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_AO_FINAL, ssaoDesc[2].getHandle());
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_GUIDEBUG, 2, &renderGuiSetting::guiDebug);
	uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_SCREEN, 2, screenSize);

	cmdList->DrawInstanced(3, 1, 0, 0);

	gui::render(cmdList.Get());

	swapchainFB[frameIndex]->closeFB(cmdList);
	
	render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

	render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

	//AABB draw
	{
		render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_AABBDEBUGDRAW);

		swapchainFB[frameIndex]->openFB(cmdList, false);

		e_globWorld.drawWorld(cmdList, true);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_OBJECT, objectConstDesc.getHandle());

		cmdList->IASetVertexBuffers(0, 1, &msh::getMesh(msh::MESH_CUBE)->getData()->vbs->view);
		cmdList->IASetVertexBuffers(1, 1, &AABBwireframeBuffer->view);
		cmdList->IASetIndexBuffer(&msh::getMesh(msh::MESH_CUBE)->getData()->idxLine->view);

		cmdList->DrawIndexedInstanced(48, e_globWorld.objectNum, 0, 0, 0);

		swapchainFB[frameIndex]->closeFB(cmdList);
		render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });
	}

	TC_CONDITION(swapChain->Present(vsync, 0) == S_OK, "Failed to present the swapchain");

	//signal the queue graphics fence and wait for it.
	render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

	frameIndex = swapChain->GetCurrentBackBufferIndex();
}
