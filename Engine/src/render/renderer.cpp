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

#include <algorithm>
#include <cmath>
#include <cstdio>

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
	bool hzbCullEnabled = true;
	bool clusterCullEnabled = true;
	bool triCullEnabled = true;
}

void bindglobalBuffers()
{
//	descriptor* clustersizeUAV = localClusterSizeBuffer->getDesc(buf::GBF_UAV);
//	descriptor* clusteroffsetUAV = localClusterOffsetBuffer->getDesc(buf::GBF_UAV);
//	descriptor* clusteroffsetSRV = localClusterOffsetBuffer->getDesc(buf::GBF_SRV);
//	descriptor* meshInfoBufferSRV = ubManager->meshInfoBuffer->getDesc(buf::GBF_SRV);
//	descriptor* lodInfoBufferSRV = ubManager->lodInfoBuffer->getDesc(buf::GBF_SRV);
//	descriptor* clusterInfoBufferSRV = ubManager->clusterInfoBuffer->getDesc(buf::GBF_SRV);
//	descriptor* vertexIDBufferSRV = ubManager->vertexIDBuffer->getDesc(buf::GBF_SRV);
//	descriptor* vertexIDBufferUAV = ubManager->vertexIDBuffer->getDesc(buf::GBF_UAV);
//	descriptor* clusterBoundBufferSRV = ubManager->clusterBoundBuffer->getDesc(buf::GBF_SRV);
//	descriptor* viewInfoBufferSRV = viewInfoBuffer->getDesc(buf::GBF_SRV);
//#if	ENGINE_DEBUG_BUFFER
//	descriptor* outDebugBufferUAV = outDebugBuffer->getDesc(buf::GBF_UAV);
//#endif // #if ENGINE_DEBUG_BUFFER
//	descriptor* unifiedVertexBufferSRV = ubManager->unifiedVertexBuffer->getDesc(buf::GBF_SRV);
//	descriptor* unifiedIndexBufferSRV = ubManager->unifiedIndexBuffer->getDesc(buf::GBF_SRV);
//	descriptor* camDesc = e_globWorld.getMainCam()->getDesc();
//
//	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VERTEX_BUFFER, unifiedVertexBufferSRV->getHandle());
//	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_INDEX_BUFFER, unifiedIndexBufferSRV->getHandle());
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
	localClusterSizeBuffer = e_globBufAllocator.alloc(nullptr, sizeof(uint) * 16, 1, buf::GBF_UAV, 0);
	occludedClusterBuffer = e_globBufAllocator.alloc(nullptr, MAX_CLUSTERS * sizeof(uint) * 3, 1, buf::GBF_UAV | buf::GBF_SRV, 0);
	debugStatsBuffer = e_globBufAllocator.alloc(nullptr, sizeof(uint) * 32, 1, buf::GBF_UAV, 0);
	debugStatsReadback = e_globBufAllocator.alloc(nullptr, sizeof(uint) * 32, 1, 0, buf::RESOURCE_READBACK);
	clusterArgsBuffer = e_globBufAllocator.alloc(nullptr, (MAX_CLUSTERS / THREADS_NUM_CLUSTERS) * sizeof(uint), 1, buf::GBF_UAV, 0);
	visibleTriBuffer = e_globBufAllocator.alloc(nullptr, MAX_CLUSTERS * THREADS_NUM_CLUSTERS * sizeof(uint), 1, buf::GBF_UAV | buf::GBF_SRV, 0);
	viewInfoBuffer = e_globBufAllocator.alloc(nullptr, MAX_OBJECTS * sizeof(float) * 10, 1, buf::GBF_SRV, buf::RESOURCE_UPLOAD, DXGI_FORMAT_R32_TYPELESS);
	materialBuffer = e_globBufAllocator.alloc(nullptr, MAX_OBJECTS * sizeof(float) * 5, 1, buf::GBF_SRV, buf::RESOURCE_UPLOAD, DXGI_FORMAT_R32_TYPELESS);
	
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
		fbDepth = e_globBufAllocator.alloc(nullptr, 0, 1, buf::GBF_DEPTH_STENCIL, buf::RESOURCE_DEPTH | buf::RESOURCE_TEXTURE | buf::RESOURCE_CLEAR, DXGI_FORMAT_D32_FLOAT, sWidth, sHeight, 1);
	}

	{
		hzbMipCount = (uint)std::floor(std::log2((double)(std::max)(sWidth, sHeight))) + 1;

		hzbDepth = e_globBufAllocator.alloc(nullptr, 0, 1, buf::GBF_SRV | buf::GBF_UAV, buf::RESOURCE_TEXTURE, DXGI_FORMAT_R32_FLOAT, sWidth, sHeight, (UINT16)hzbMipCount);

		hzbMipUAV.clear();
		hzbMipSRV.clear();
		hzbMipUAV.reserve(hzbMipCount);
		hzbMipSRV.reserve(hzbMipCount);
		hzbMipState.assign(hzbMipCount, D3D12_RESOURCE_STATE_COMMON);

		for (uint m = 0; m < hzbMipCount; ++m)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = m;
			uavDesc.Texture2D.PlaneSlice = 0;

			hzbMipUAV.push_back(render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_UAV_TYPE, hzbDepth, &uavDesc));

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MostDetailedMip = m;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0;

			hzbMipSRV.push_back(render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, hzbDepth, &srvDesc));
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC fullSrvDesc = {};
		fullSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		fullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		fullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		fullSrvDesc.Texture2D.MostDetailedMip = 0;
		fullSrvDesc.Texture2D.MipLevels = hzbMipCount;
		fullSrvDesc.Texture2D.PlaneSlice = 0;
		fullSrvDesc.Texture2D.ResourceMinLODClamp = 0;

		hzbFullSRV = render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_IMAGE_TYPE, hzbDepth, &fullSrvDesc);

		hzbReady = false;
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
	//objID
	gbufferFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//objInfo
	gbufferFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//visID
	gbufferFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32G32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	gbufferFB->attachDepth(fbDepth, 0.0f);

#if ENGINE_DEBUG_DEBUGCAM
	//should be sync with gbufferFB
	gbufferDebugFB = new framebuffer();
	//position
	gbufferDebugFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32G32B32A32_FLOAT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//normal
	gbufferDebugFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//objID
	gbufferDebugFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//debugID
	gbufferDebugFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	//visID
	gbufferDebugFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R32G32_UINT, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	gbufferDebugFB->attachDepth(fbDepth, 0.0f);
#endif // #if ENGINE_DEBUG_DEBUGCAM

	debugFB = new framebuffer();
	debugFB->createAddFBO(sWidth, sHeight, DXGI_FORMAT_R8_UNORM, DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
	debugFB->attachDepth(fbDepth, 0.0f);

	{
		std::vector<render::cmdSigData> sigData[3];

		//sigData[0].push_back({ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT, CBV_SCREEN, 1 });

		render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->setCommandSignature(sigData[0]);
		render::getpipelinestate(render::PSO_CULLCLUSTER)->setCommandSignature(sigData[1]);
		render::getpipelinestate(render::PSO_RASTERIZER)->setCommandSignature(sigData[2]);
		render::getpipelinestate(render::PSO_CULLCLUSTER_POST)->setCommandSignature(sigData[1]);
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

	{
		render::ScopedGPUEvent uploadEvent(cmdList.Get(), "Buffer Upload");

		cmdList->CopyBufferRegion(resource, 0, uploadBuffer->getResource(), offset, size);
	}

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

#if ENGINE_DEBUG_MESH
void renderer::debugFrameBufferRequest(uint debugMeshID, UINT64 projPtr)
{
	debugFBRequest = true;
	debugFBMeshID = debugMeshID;
	debugProjection = projPtr;
}
#endif // #if ENGINE_DEBUG_MESH

const char* debugDrawVersion[]
{
	"None",
	"Position",
	"Normal",
	"Debug",
	"SSAO",
};

void renderer::guiCullingToggles()
{
	ImGui::Checkbox("HZB Cluster Occlusion", &renderGuiSetting::hzbCullEnabled);
	ImGui::Checkbox("Cluster Culling", &renderGuiSetting::clusterCullEnabled);
	ImGui::Checkbox("Triangle Culling", &renderGuiSetting::triCullEnabled);
}

void renderer::guiSetting()
{
	gui::comboBox("DebugDraw", debugDrawVersion, 5, renderGuiSetting::guiDebug.debugDraw);

	ImGui::Checkbox("SSAO", &renderGuiSetting::ssaoEnabled);
	ImGui::Checkbox("ShowAABB", &renderGuiSetting::guiDebug.AABBDraw);
	guiCullingToggles();

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

const renderer::cullStats& renderer::getCullStats() const
{
	return cullStatsData;
}

const float* renderer::getClusterSurvivorHistory() const
{
	return clusterSurvivorHistory.data();
}

const float* renderer::getTriSurvivorHistory() const
{
	return triSurvivorHistory.data();
}

uint renderer::getCullStatsHistoryOffset() const
{
	return (cullStatsHistoryHead + 1) % CULLSTATS_HISTORY;
}

uint renderer::getHZBMipCount() const
{
	return hzbMipCount;
}

D3D12_GPU_DESCRIPTOR_HANDLE renderer::getHZBMipHandle(uint mip) const
{
	uint clampedMip = hzbMipCount > 0 ? (std::min)(mip, hzbMipCount - 1) : 0;
	return hzbMipSRV[clampedMip].getHandle();
}

void renderer::getHZBMipSize(uint mip, uint& w, uint& h) const
{
	w = (std::max)(1u, e_globWindow.width() >> mip);
	h = (std::max)(1u, e_globWindow.height() >> mip);
}

void renderer::transitionHZBForGui(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

	for (uint m = 0; m < hzbMipCount; ++m)
	{
		if (hzbMipState[m] != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(hzbDepth->getResource(), hzbMipState[m], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m));
			hzbMipState[m] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
	}

	if (!barriers.empty())
	{
		cmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());
	}
}

void renderer::preDraw(float dt)
{
#if ENGINE_DEBUG_MESH
	if (debugFBRequest)
	{
		auto cmdList = render::getCmdQueue(render::QUEUE_GRAPHIC)->getCmdList();
		mesh* msh = msh::getMesh((msh::MESH_INDEX)debugFBMeshID);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_WIREFRAME);

		{
			render::ScopedGPUEvent debugMeshEvent(cmdList.Get(), "DebugMeshDraw");

			debugFB->openFB(cmdList, true);

			CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT{ 0.0f, 0.0f, (float)e_globWindow.width(), (float)e_globWindow.height() };
			CD3DX12_RECT scissorRect = CD3DX12_RECT{ 0, 0, (long)e_globWindow.width(), (long)e_globWindow.height() };

			cmdList->RSSetViewports(1, &viewport);
			cmdList->RSSetScissorRects(1, &scissorRect);

			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			//cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			//msh->setBuffer(cmdList, false);

			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, (D3D12_GPU_DESCRIPTOR_HANDLE)debugProjection);
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_DEBUG_MESHVIEW_ID, 1, &debugFBMeshID);

			descriptor* debugUnifiedVertexBufferSRV = ubManager->unifiedVertexBuffer->getDesc(buf::GBF_SRV);
			descriptor* debugUnifiedIndexBufferSRV = ubManager->unifiedIndexBuffer->getDesc(buf::GBF_SRV);
			descriptor* debugMeshInfoBufferSRV = ubManager->meshInfoBuffer->getDesc(buf::GBF_SRV);
			descriptor* debugLodInfoBufferSRV = ubManager->lodInfoBuffer->getDesc(buf::GBF_SRV);
			descriptor* debugClusterInfoBufferSRV = ubManager->clusterInfoBuffer->getDesc(buf::GBF_SRV);

			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VERTEX_BUFFER, debugUnifiedVertexBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_INDEX_BUFFER, debugUnifiedIndexBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_MESHINFO_BUFFER, debugMeshInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LOD_INFO_BUFFER, debugLodInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_CLUSTER_INFO_BUFFER, debugClusterInfoBufferSRV->getHandle());

			msh->draw(cmdList);

			debugFB->closeFB(cmdList);
		}

		render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

		render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

		debugFBRequest = false;
	}
#endif // #if ENGINE_DEBUG_MESH

	e_globWorld.instanceCulling();
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

		{
			render::ScopedGPUEvent noiseEvent(computeCmdList.Get(), "GenNoise");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_NOISE, noise->getDesc(buf::GBF_UAV)->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_NOISECONST, 2, &renderGuiSetting::noiseConstants);

			computeCmdList->Dispatch(513, 513, 1);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENTERRAIN);

		{
			render::ScopedGPUEvent terrainEvent(computeCmdList.Get(), "GenTerrain");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TERRAIN_VERT, terrainVert[0]->getDesc(buf::GBF_UAV)->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TERRAIN_NORM, terrainVert[1]->getDesc(buf::GBF_UAV)->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_TERRAIN_NOISE, noise->getDesc(buf::GBF_SRV)->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_TERRAINCONST, 1, &renderGuiSetting::terrainConstants);

			computeCmdList->Dispatch(74, 74, 1);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_GENTERRAININDEX);

		{
			render::ScopedGPUEvent terrainIndexEvent(computeCmdList.Get(), "GenTerrainIndex");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TERRAIN_INDEX, terrainVert[2]->getDesc(buf::GBF_UAV)->getHandle());

			computeCmdList->Dispatch(64, 64, 1);
		}

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
	setUpTerrain();

	{
		//todo stride
		cmdConstBuffer = e_globBufAllocator.alloc(nullptr, 513 * sizeof(uint), 6, buf::GBF_CBV, buf::RESOURCE_UPLOAD);
	}

#if ENGINE_DEBUG_DEBUGCAM
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
#endif // #if ENGINE_DEBUG_DEBUGCAM

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
}

void renderer::generateHZB()
{
	auto hzbCmdList = render::getCmdQueue(render::QUEUE_GRAPHIC)->getCmdList();

	render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_GENHZB);

	{
		render::ScopedGPUEvent hzbEvent(hzbCmdList.Get(), "Generate HZB");

		{
			render::ScopedGPUEvent hzbCopyEvent(hzbCmdList.Get(), "HZB Copy Depth");

			CD3DX12_RESOURCE_BARRIER preCopyBarriers[2];

			preCopyBarriers[0] = fbDepth->getTransition(D3D12_RESOURCE_STATE_COPY_SOURCE);
			preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(hzbDepth->getResource(), hzbMipState[0], D3D12_RESOURCE_STATE_COPY_DEST, 0);
			hzbMipState[0] = D3D12_RESOURCE_STATE_COPY_DEST;

			hzbCmdList->ResourceBarrier(2, preCopyBarriers);

			CD3DX12_TEXTURE_COPY_LOCATION srcLoc(fbDepth->getResource(), 0);
			CD3DX12_TEXTURE_COPY_LOCATION dstLoc(hzbDepth->getResource(), 0);

			hzbCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

			CD3DX12_RESOURCE_BARRIER postCopyBarriers[2];

			postCopyBarriers[0] = fbDepth->getTransition(D3D12_RESOURCE_STATE_DEPTH_WRITE);
			postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(hzbDepth->getResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0);
			hzbMipState[0] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			hzbCmdList->ResourceBarrier(2, postCopyBarriers);
		}

		for (uint m = 1; m < hzbMipCount; ++m)
		{
			char hzbEventName[32];
			snprintf(hzbEventName, sizeof(hzbEventName), "genHZB Mip %u", m);

			render::ScopedGPUEvent hzbMipEvent(hzbCmdList.Get(), hzbEventName);

			CD3DX12_RESOURCE_BARRIER dstBarrier = CD3DX12_RESOURCE_BARRIER::Transition(hzbDepth->getResource(), hzbMipState[m], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, m);
			hzbMipState[m] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

			hzbCmdList->ResourceBarrier(1, &dstBarrier);

			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_HZB_SRC, hzbMipSRV[m - 1].getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(UAV_HZB_DST, hzbMipUAV[m].getHandle());

			uint dstW = (std::max)(1u, e_globWindow.width() >> m);
			uint dstH = (std::max)(1u, e_globWindow.height() >> m);

			uint texSize[3] = { dstW, dstH, 1 };
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_HZBCONST, 3, texSize);

			hzbCmdList->Dispatch((dstW + 7) / 8, (dstH + 7) / 8, 1);

			CD3DX12_RESOURCE_BARRIER srcBarrier = CD3DX12_RESOURCE_BARRIER::Transition(hzbDepth->getResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, m);
			hzbMipState[m] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			hzbCmdList->ResourceBarrier(1, &srcBarrier);
		}
	}

	render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ hzbCmdList });

	render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

	hzbReady = true;
}

void renderer::draw(float dt)
{
	unsigned char* viewInfoDataPtr = nullptr;
	unsigned char* materialDataPtr = nullptr;
	viewInfoBuffer->mapBuffer(&viewInfoDataPtr);
	materialBuffer->mapBuffer(&materialDataPtr);
	e_globWorld.uploadObjectInfo(viewInfoDataPtr, materialDataPtr);
	viewInfoBuffer->unmapBuffer();
	materialBuffer->unmapBuffer();

	descriptor* clustersizeUAV = localClusterSizeBuffer->getDesc(buf::GBF_UAV);
	descriptor* clusteroffsetUAV = localClusterOffsetBuffer->getDesc(buf::GBF_UAV);
	descriptor* clusteroffsetSRV = localClusterOffsetBuffer->getDesc(buf::GBF_SRV);
	descriptor* occludedClusterBufferUAV = occludedClusterBuffer->getDesc(buf::GBF_UAV);
	descriptor* occludedClusterBufferSRV = occludedClusterBuffer->getDesc(buf::GBF_SRV);
	descriptor* debugStatsUAV = debugStatsBuffer->getDesc(buf::GBF_UAV);
	descriptor* meshInfoBufferSRV = ubManager->meshInfoBuffer->getDesc(buf::GBF_SRV);
	descriptor* lodInfoBufferSRV = ubManager->lodInfoBuffer->getDesc(buf::GBF_SRV);
	descriptor* clusterInfoBufferSRV = ubManager->clusterInfoBuffer->getDesc(buf::GBF_SRV);
	descriptor* vertexIDBufferSRV = ubManager->vertexIDBuffer->getDesc(buf::GBF_SRV);
	descriptor* vertexIDBufferUAV = ubManager->vertexIDBuffer->getDesc(buf::GBF_UAV);
	descriptor* visibleTriBufferUAV = visibleTriBuffer->getDesc(buf::GBF_UAV);
	descriptor* visibleTriBufferSRV = visibleTriBuffer->getDesc(buf::GBF_SRV);
	descriptor* clusterBoundBufferSRV = ubManager->clusterBoundBuffer->getDesc(buf::GBF_SRV);
	descriptor* viewInfoBufferSRV = viewInfoBuffer->getDesc(buf::GBF_SRV);
	descriptor* materialBufferSRV = materialBuffer->getDesc(buf::GBF_SRV);
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

	bool hzbCullEnable = false;

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_INITCLUSTER);

		{
			render::ScopedGPUEvent initClusterEvent(computeCmdList.Get(), "InitCluster");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CULLING_DEBUG_STATS, debugStatsUAV->getHandle());

			computeCmdList->Dispatch(1, 1, 1);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_UPLOADLOCALOBJ);

		{
			render::ScopedGPUEvent uploadObjEvent(computeCmdList.Get(), "UploadLocalObj");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTEROFFSET_BUFFER, clusteroffsetUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());

			unsigned char* cbvDataBegin;
			cmdConstBuffer->mapBuffer(&cbvDataBegin);
			uint objCount = e_globWorld.submitObjects(cbvDataBegin);
			uint postOffset = objCount;
			uint postCount = e_globWorld.cameraObjNum[1];

			memcpy(cbvDataBegin + 64 * 4 * 4, &objCount, 4);
			memcpy(cbvDataBegin + 64 * 4 * 4 + 4, &postOffset, 4);
			memcpy(cbvDataBegin + 64 * 4 * 4 + 8, &postCount, 4);

			cmdConstBuffer->unmapBuffer();

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_CMDBUFCONSTS, cmdConstBuffer->getDesc(buf::GBF_CBV)->getHandle());

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_MESHINFO_BUFFER, meshInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LOD_INFO_BUFFER, lodInfoBufferSRV->getHandle());

			if (objCount > 0)
			{
				computeCmdList->Dispatch(objCount, 1, 1);
			}
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_CULLCLUSTER);

		{
			render::ScopedGPUEvent cullClusterEvent(computeCmdList.Get(), "CullCluster");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LOD_INFO_BUFFER, lodInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_INFO_BUFFER, clusterInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_ARGS_BUFFER, clusteroffsetSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERARGS_BUFFER, vertexIDBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_BOUNDS_BUFFER, clusterBoundBufferSRV->getHandle());

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_VIEWINFO_BUFFER, viewInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, camDesc->getHandle());

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CULLING_HZB, hzbFullSRV.getHandle());

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_OCCLUDED_CLUSTERS, occludedClusterBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CULLING_DEBUG_STATS, debugStatsUAV->getHandle());

			hzbCullEnable = hzbReady && renderGuiSetting::hzbCullEnabled && e_globWorld.getMainCam()->hasPrevViewProj();
			uint hzbConsts[4] = { e_globWindow.width(), e_globWindow.height(), hzbMipCount, hzbCullEnable ? 1u : 0u };
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_CULLING_HZBCONST, 4, hzbConsts);

			uint clusterCullFlag = renderGuiSetting::clusterCullEnabled ? 1u : 0u;
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_CULLING_DEBUG, 1, &clusterCullFlag);

#if ENGINE_DEBUG_BUFFER
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_GLOBAL_DEBUG_BUFFER, outDebugBufferUAV->getHandle());
#endif // #if ENGINE_DEBUG_BUFFER

			computeCmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_CULLCLUSTER)->getCmdSignature(), 1, localClusterSizeBuffer->getResource(), sizeof(uint), nullptr, 0);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_RASTERIZER);

		{
			render::ScopedGPUEvent rasterizerEvent(computeCmdList.Get(), "Rasterizer");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERARGS_BUFFER, vertexIDBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_VISIBLE_TRI_BUFFER, visibleTriBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CULLING_DEBUG_STATS, debugStatsUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_VERTEX_BUFFER, unifiedVertexBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_INDEX_BUFFER, unifiedIndexBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_MESHINFO_BUFFER, meshInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_VIEWINFO_BUFFER, viewInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, camDesc->getHandle());
			uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_SCREEN, 2, screenSize);

			uint triCullFlag = renderGuiSetting::triCullEnabled ? 1u : 0u;
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_RASTER_DEBUG, 1, &triCullFlag);

#if ENGINE_DEBUG_BUFFER
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_GLOBAL_DEBUG_BUFFER, outDebugBufferUAV->getHandle());
#endif // #if ENGINE_DEBUG_BUFFER

			computeCmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_RASTERIZER)->getCmdSignature(), 1, localClusterSizeBuffer->getResource(), 9 * sizeof(uint), nullptr, 0);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

#if ENGINE_DEBUG_DEBUGCAM
	if (e_globWorld.getMainCam()->viewportType != cam::VIEWPORT_FULL)
	{
		auto cmdList = render::getCmdQueue(render::QUEUE_GRAPHIC)->getCmdList();

		render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_GBUFFERINDIRECT);

		gbufferDebugFB->openFB(cmdList, true);

		e_globWorld.setupCam(cmdList, false, true);

		{
			render::ScopedGPUEvent gbufferDebugEvent(cmdList.Get(), "Draw GBuffer (Debug Cam)");

			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VERTEX_BUFFER, unifiedVertexBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_INDEX_BUFFER, unifiedIndexBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_CLUSTERARGS, vertexIDBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_VISIBLE_TRIS, visibleTriBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VIEWINFO_BUFFER, viewInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_MESHINFO_BUFFER, meshInfoBufferSRV->getHandle());

			cmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->getCmdSignature(), 1, localClusterSizeBuffer->getResource(), 5 * sizeof(uint), nullptr, 0);

			gbufferDebugFB->closeFB(cmdList);
		}

		render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

		render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();
	}
#endif // #if ENGINE_DEBUG_DEBUGCAM

	auto cmdList = render::getCmdQueue(render::QUEUE_GRAPHIC)->getCmdList();

	render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_GBUFFERINDIRECT);

	gbufferFB->openFB(cmdList, true);

	e_globWorld.setupCam(cmdList, true, true);

	{
		render::ScopedGPUEvent gbufferEvent(cmdList.Get(), "Draw GBuffer");

		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VERTEX_BUFFER, unifiedVertexBufferUAV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_INDEX_BUFFER, unifiedIndexBufferUAV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_CLUSTERARGS, vertexIDBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_VISIBLE_TRIS, visibleTriBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VIEWINFO_BUFFER, viewInfoBufferSRV->getHandle());
		render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_MESHINFO_BUFFER, meshInfoBufferSRV->getHandle());

		cmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->getCmdSignature(), 1, localClusterSizeBuffer->getResource(), 5 * sizeof(uint), nullptr, 0);
	}

	gbufferFB->closeFB(cmdList);

	render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

	render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

	generateHZB();

	if (e_globWorld.cameraObjNum[1] > 0)
	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_UPLOADPOSTOBJ);

		{
			render::ScopedGPUEvent uploadPostObjEvent(computeCmdList.Get(), "UploadPostObj");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_OCCLUDED_CLUSTERS, occludedClusterBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_CMDBUFCONSTS, cmdConstBuffer->getDesc(buf::GBF_CBV)->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_MESHINFO_BUFFER, meshInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LOD_INFO_BUFFER, lodInfoBufferSRV->getHandle());

			const uint CLUSTER_THREAD_NUM_CPU = 64;
			uint postCount = e_globWorld.cameraObjNum[1];
			computeCmdList->Dispatch((postCount + CLUSTER_THREAD_NUM_CPU - 1) / CLUSTER_THREAD_NUM_CPU, 1, 1);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_PREPPOSTARGS);

		{
			render::ScopedGPUEvent prepPostArgsEvent(computeCmdList.Get(), "PrepPostArgs");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CULLING_DEBUG_STATS, debugStatsUAV->getHandle());

			computeCmdList->Dispatch(1, 1, 1);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_CULLCLUSTER_POST);

		{
			render::ScopedGPUEvent cullClusterPostEvent(computeCmdList.Get(), "CullClusterPost");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LOD_INFO_BUFFER, lodInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_INFO_BUFFER, clusterInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_ARGS_BUFFER, occludedClusterBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERARGS_BUFFER, vertexIDBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CLUSTER_BOUNDS_BUFFER, clusterBoundBufferSRV->getHandle());

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_VIEWINFO_BUFFER, viewInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, camDesc->getHandle());

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_CULLING_HZB, hzbFullSRV.getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CULLING_DEBUG_STATS, debugStatsUAV->getHandle());

			uint hzbPostConsts[4] = { e_globWindow.width(), e_globWindow.height(), hzbMipCount, renderGuiSetting::hzbCullEnabled ? 1u : 0u };
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_CULLING_HZBCONST, 4, hzbPostConsts);

			uint clusterCullPostFlag = renderGuiSetting::clusterCullEnabled ? 1u : 0u;
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_CULLING_DEBUG, 1, &clusterCullPostFlag);

#if ENGINE_DEBUG_BUFFER
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_GLOBAL_DEBUG_BUFFER, outDebugBufferUAV->getHandle());
#endif // #if ENGINE_DEBUG_BUFFER

			computeCmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_CULLCLUSTER_POST)->getCmdSignature(), 1, localClusterSizeBuffer->getResource(), 13 * sizeof(uint), nullptr, 0);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_RASTERIZER);

		{
			render::ScopedGPUEvent rasterizerPostEvent(computeCmdList.Get(), "Rasterizer (Post)");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERARGS_BUFFER, vertexIDBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CLUSTERSIZE_BUFFER, clustersizeUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_VISIBLE_TRI_BUFFER, visibleTriBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_CULLING_DEBUG_STATS, debugStatsUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_VERTEX_BUFFER, unifiedVertexBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_INDEX_BUFFER, unifiedIndexBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_MESHINFO_BUFFER, meshInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_VIEWINFO_BUFFER, viewInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, camDesc->getHandle());
			uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_SCREEN, 2, screenSize);

			uint triCullPostFlag = renderGuiSetting::triCullEnabled ? 1u : 0u;
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_RASTER_DEBUG, 1, &triCullPostFlag);

#if ENGINE_DEBUG_BUFFER
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_GLOBAL_DEBUG_BUFFER, outDebugBufferUAV->getHandle());
#endif // #if ENGINE_DEBUG_BUFFER

			computeCmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_RASTERIZER)->getCmdSignature(), 1, localClusterSizeBuffer->getResource(), 9 * sizeof(uint), nullptr, 0);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();

		// End-of-frame debug-stats readback: at this point [0..7] hold pass-2's
		// live counters and [8..15] hold pass-1's snapshot taken by
		// prepPostArgs_cs earlier this frame. The compute list was already
		// Close()'d by the execute() above, so it must be reopened via
		// bindPSO before recording anything else into it.
		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_RASTERIZER);

		{
			render::ScopedGPUEvent debugStatsCopyEvent(computeCmdList.Get(), "Copy DebugStats Readback");

			CD3DX12_RESOURCE_BARRIER toCopySrc = CD3DX12_RESOURCE_BARRIER::Transition(
				debugStatsBuffer->getResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
			computeCmdList->ResourceBarrier(1, &toCopySrc);

			computeCmdList->CopyBufferRegion(debugStatsReadback->getResource(), 0, debugStatsBuffer->getResource(), 0, sizeof(uint) * 32);

			CD3DX12_RESOURCE_BARRIER toUAV = CD3DX12_RESOURCE_BARRIER::Transition(
				debugStatsBuffer->getResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			computeCmdList->ResourceBarrier(1, &toUAV);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();

		{
			ID3D12Resource* statsResource = debugStatsReadback->getResource();
			CD3DX12_RANGE statsReadRange(0, sizeof(uint) * 32);
			unsigned char* statsPtr = nullptr;
			HRESULT statsMapResult = statsResource->Map(0, &statsReadRange, reinterpret_cast<void**>(&statsPtr));

			if (SUCCEEDED(statsMapResult))
			{
				uint slots[32] = {};
				memcpy(slots, statsPtr, sizeof(uint) * 32);

				CD3DX12_RANGE statsWrittenRange(0, 0);
				statsResource->Unmap(0, &statsWrittenRange);

				cullStatsData.pass1.clusterCandidates = slots[8];
				cullStatsData.pass1.clusterFrustumCulled = slots[9];
				cullStatsData.pass1.clusterOccluded = slots[10];
				cullStatsData.pass1.clusterSurvivors = slots[11];
				cullStatsData.pass1.triCandidates = slots[12];
				cullStatsData.pass1.triSurvivors = slots[13];

				cullStatsData.pass2.clusterCandidates = slots[0];
				cullStatsData.pass2.clusterFrustumCulled = slots[1];
				cullStatsData.pass2.clusterOccluded = slots[2];
				cullStatsData.pass2.clusterSurvivors = slots[3];
				cullStatsData.pass2.triCandidates = slots[4];
				cullStatsData.pass2.triSurvivors = slots[5];

				cullStatsData.instancesTotal = e_globWorld.objectNum;
				cullStatsData.instancesPass1 = e_globWorld.cameraObjNum[0];
				cullStatsData.instancesPass2 = e_globWorld.cameraObjNum[1];
				cullStatsData.hzbActive = hzbCullEnable;

				float survivorTotal = (float)(cullStatsData.pass1.clusterSurvivors + cullStatsData.pass2.clusterSurvivors);
				float triSurvivorTotal = (float)(cullStatsData.pass1.triSurvivors + cullStatsData.pass2.triSurvivors);

				cullStatsHistoryHead = (cullStatsHistoryHead + 1) % CULLSTATS_HISTORY;
				clusterSurvivorHistory[cullStatsHistoryHead] = survivorTotal;
				triSurvivorHistory[cullStatsHistoryHead] = triSurvivorTotal;

				if ((clusterStatsFrameCounter % 60) == 0)
				{
					char statsLog[512];
					snprintf(statsLog, sizeof(statsLog),
						"[ClusterStats] pass1: candidates=%u frustumCulled=%u occluded=%u survivors=%u triCandidates=%u triSurvivors=%u | pass2: candidates=%u frustumCulled=%u occluded=%u survivors=%u triCandidates=%u triSurvivors=%u | instances total=%u pass1=%u pass2=%u hzbEnabled=%u",
						cullStatsData.pass1.clusterCandidates, cullStatsData.pass1.clusterFrustumCulled, cullStatsData.pass1.clusterOccluded, cullStatsData.pass1.clusterSurvivors, cullStatsData.pass1.triCandidates, cullStatsData.pass1.triSurvivors,
						cullStatsData.pass2.clusterCandidates, cullStatsData.pass2.clusterFrustumCulled, cullStatsData.pass2.clusterOccluded, cullStatsData.pass2.clusterSurvivors, cullStatsData.pass2.triCandidates, cullStatsData.pass2.triSurvivors,
						cullStatsData.instancesTotal, cullStatsData.instancesPass1, cullStatsData.instancesPass2, cullStatsData.hzbActive ? 1u : 0u);
					TC_LOG_INFO(statsLog);
				}
			}
			else
			{
				TC_LOG_ERROR("Failed to map DebugStats readback buffer");
			}
		}

		++clusterStatsFrameCounter;
	}

	{
		render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_GBUFFERINDIRECT);

		gbufferFB->openFB(cmdList, false);

		e_globWorld.setupCam(cmdList, true, true);

		{
			render::ScopedGPUEvent gbufferPostEvent(cmdList.Get(), "Draw GBuffer (Post)");

			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VERTEX_BUFFER, unifiedVertexBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_INDEX_BUFFER, unifiedIndexBufferUAV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_CLUSTERARGS, vertexIDBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_GBUFFER_VISIBLE_TRIS, visibleTriBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_VIEWINFO_BUFFER, viewInfoBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_MESHINFO_BUFFER, meshInfoBufferSRV->getHandle());

			cmdList->ExecuteIndirect(render::getpipelinestate(render::PSO_GBUFFERINDIRECT)->getCmdSignature(), 1, localClusterSizeBuffer->getResource(), 5 * sizeof(uint), nullptr, 0);
		}

		gbufferFB->closeFB(cmdList);

		render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

		render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();
	}

	generateHZB();

	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(render::PSO_SSAO);

		{
			render::ScopedGPUEvent ssaoEvent(computeCmdList.Get(), "Draw SSAO");

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LIGHT_POSITION, gbufferFB->getDescHandle(0));
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LIGHT_NORM, gbufferFB->getDescHandle(1));
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LIGHT_OBJID, gbufferFB->getDescHandle(2));
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, camDesc->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAO, ssaoUAV[0]->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_AOCONST, 4, &renderGuiSetting::aoConstants);
			uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_SCREEN, 2, screenSize);

			computeCmdList->Dispatch(e_globWindow.width() / 8, e_globWindow.height() / 8, 1);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	static constexpr const char* ssaoBlurNames[2] = { "SSAO_BlurHorizontal", "SSAO_BlurVertical" };

	for(uint i = 0; i < 2; ++i)
	{
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();

		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO((render::PSO_INDEX)((uint)render::PSO_SSAOBLURX + i));

		{
			render::ScopedGPUEvent ssaoBlurEvent(computeCmdList.Get(), ssaoBlurNames[i]);

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LIGHT_POSITION, gbufferFB->getDescHandle(0));
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LIGHT_NORM, gbufferFB->getDescHandle(1));
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_LIGHT_OBJID, gbufferFB->getDescHandle(2));
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_PROJECTION, camDesc->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAO, ssaoUAV[i]->getHandle());
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_SSAOBLUR, ssaoUAV[i + 1]->getHandle());
			uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_SCREEN, 2, screenSize);

			computeCmdList->Dispatch(e_globWindow.width() / 8, e_globWindow.height() / 8, 1);
		}

		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });

		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();
	}

	render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_PBR);

	{
		render::ScopedGPUEvent lightEvent(cmdList.Get(), "Draw light");

		swapchainFB[frameIndex]->openFB(cmdList, true);

#if ENGINE_DEBUG_DEBUGCAM
		if (e_globWorld.getMainCam()->viewportType != cam::VIEWPORT_FULL)
		{
			render::ScopedGPUEvent pbrDebugEvent(cmdList.Get(), "PBR Debug View");

			e_globWorld.setupCam(cmdList, false, false);

			setVertexBuffer(cmdList, 0, sceneTriangleBuffer);

			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_POSITION, gbufferDebugFB->getDescHandle(0));
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_NORM, gbufferDebugFB->getDescHandle(1));
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_OBJID, gbufferDebugFB->getDescHandle(2));
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_DEBUG, gbufferDebugFB->getDescHandle(3));
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_MATERIAL_BUFFER, materialBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, camDesc->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_AO, ssaoSRV[2]->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_GUIDEBUG, 2, &renderGuiSetting::guiDebug);
			uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_SCREEN, 2, screenSize);

			cmdList->DrawInstanced(3, 1, 0, 0);
		}
#endif // #if ENGINE_DEBUG_DEBUGCAM

		{
			render::ScopedGPUEvent pbrEvent(cmdList.Get(), "PBR");

			e_globWorld.setupCam(cmdList, true, false);

			setVertexBuffer(cmdList, 0, sceneTriangleBuffer);

			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_POSITION, gbufferFB->getDescHandle(0));
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_NORM, gbufferFB->getDescHandle(1));
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_OBJID, gbufferFB->getDescHandle(2));
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_DEBUG, gbufferFB->getDescHandle(3));
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_MATERIAL_BUFFER, materialBufferSRV->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, camDesc->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(SRV_LIGHT_AO, ssaoSRV[2]->getHandle());
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_GUIDEBUG, 2, &renderGuiSetting::guiDebug);
			uint screenSize[2] = { e_globWindow.width(), e_globWindow.height() };
			render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_SCREEN, 2, screenSize);

			cmdList->DrawInstanced(3, 1, 0, 0);
		}

		transitionHZBForGui(cmdList);

		{
			render::ScopedGPUEvent imguiEvent(cmdList.Get(), "ImGui");

			gui::render(cmdList.Get());
		}

		swapchainFB[frameIndex]->closeFB(cmdList);
	}

	render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });

	render::getCmdQueue(render::QUEUE_GRAPHIC)->flush();

	if(renderGuiSetting::guiDebug.AABBDraw)
	{
		unsigned char* aabbGPUAddress = nullptr;
		AABBwireframeBuffer[1]->mapBuffer(&aabbGPUAddress);

		e_globWorld.boundData(aabbGPUAddress);

		AABBwireframeBuffer[1]->unmapBuffer();

		render::getCmdQueue(render::QUEUE_GRAPHIC)->bindPSO(render::PSO_AABBDEBUGDRAW);

		{
			render::ScopedGPUEvent aabbEvent(cmdList.Get(), "Draw AABB");

			swapchainFB[frameIndex]->openFB(cmdList, false);

			e_globWorld.setupCam(cmdList, true, false);

			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

			setVertexBuffer(cmdList, 0, AABBwireframeBuffer[0]);
			setVertexBuffer(cmdList, 1, AABBwireframeBuffer[1]);
			setIndexBuffer(cmdList, AABBwireframeBuffer[2]);

			cmdList->DrawIndexedInstanced(24, e_globWorld.objectNum, 0, 0, 0);

			swapchainFB[frameIndex]->closeFB(cmdList);
		}

		render::getCmdQueue(render::QUEUE_GRAPHIC)->execute({ cmdList });
	}

	TC_CONDITION(SUCCEEDED(swapChain->Present(vsync, 0)), "Failed to present the swapchain");

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