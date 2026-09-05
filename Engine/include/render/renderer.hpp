#pragma once
#include <array>
#include <vector>
#include <system/defines.hpp>
#include <render/descriptorheap.hpp>
#include <render/buffer.hpp>
#include <render/render_UB.hpp>
#include <render/render_debug.hpp>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>

#include <DirectXMath.h>

class framebuffer;
namespace render
{
	class UBManager;
}

constexpr uint FRAME_COUNT = 2;

struct meshInfo
{
	uint lodOffset;
	uint numLod;
	uint vertexOffset;
	uint flags = 0;
};

class renderBuf
{
public:
//put GPU buffer
	buffer* UVB;
	buffer* UIB;
};

class renderer
{
public:
	bool init(Microsoft::WRL::ComPtr<IDXGIFactory4> dxFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter);
	void setUp();
	void draw(float dt);
	void drawWorld(float dt);
	void close();

	void preDraw(float dt);

	Microsoft::WRL::ComPtr<ID3D12Device2> device;

#if ENGINE_DEBUG_READBACK
	renderDebug debugSubsystem;
#endif // ENGINE_DEBUG_READBACK
private:
	bool createDevice(Microsoft::WRL::ComPtr<IDXGIFactory4> dxFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter);
	bool checkFeatureSupport(DXGI_FEATURE feature);
	bool createSwapChain();
	bool createFrameResources();
	void generateHZB();

private:
	//created from engine
	Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
#if ENGINE_DEBUG_RESOURCEVIEW
	Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
#endif // ENGINE_DEBUG_RESOURCEVIEW

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;

	Microsoft::WRL::ComPtr<ID3D12CommandSignature> cmdSignature;
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> cmdprepassSignature;

private:
	framebuffer* swapchainFB[FRAME_COUNT];
	framebuffer* gbufferFB = nullptr;
	framebuffer* visBufferFB = nullptr;
#if ENGINE_DEBUG_CLEARGBUFFER
	framebuffer* gbufferClearFB = nullptr;
#endif // #if ENGINE_DEBUG_CLEARGBUFFER
#if ENGINE_DEBUG_DEBUGCAM
	framebuffer* gbufferDebugFB = nullptr;
#endif // #if ENGINE_DEBUG_DEBUGCAM

#if ENGINE_DEBUG_MESH
	framebuffer* debugFB = nullptr;
	bool debugFBRequest = false;
	uint debugFBMeshID;
	buffer* debugProjection = nullptr;
#endif // #if ENGINE_DEBUG_MESH

	buffer* ssaoTex[3];

	buffer* commandBuffer = nullptr;
	buffer* objectConstBuffer = nullptr;
	buffer* localClusterOffsetBuffer = nullptr;
	buffer* localClusterSizeBuffer = nullptr;
	buffer* occludedClusterBuffer = nullptr;
	buffer* clusterIndirectionBuffer = nullptr;
	buffer* debugStatsBuffer = nullptr;
	uint clusterStatsFrameCounter = 0;
	buffer* materialPixelCountsBuffer = nullptr;
	buffer* materialMemoryOffsetBuffer = nullptr;
	buffer* materialPixelArgsBuffer = nullptr;
	buffer* materialBlockCursorBuffer = nullptr;
	buffer* materialPixelInfoBuffer = nullptr;
	buffer* materialGbufferArgsBuffer = nullptr;
	buffer* gbufferPositionTex = nullptr;
	buffer* gbufferNormalTex = nullptr;
	buffer* clusterArgsBuffer = nullptr;
	buffer* visibleTriBuffer = nullptr;
	buffer* fbDepth = nullptr;

	buffer* hzbDepth = nullptr;
	uint hzbMipCount = 0;
	std::vector<buffer*> hzbMipBuffer;
	std::vector<D3D12_RESOURCE_STATES> hzbMipState;
	bool hzbReady = false;

#if	ENGINE_DEBUG_BUFFER
	buffer* outDebugBuffer = nullptr;
#endif //#if ENGINE_DEBUG_BUFFER
	buffer* viewInfoBuffer = nullptr;
	buffer* materialBuffer = nullptr;
	buffer* clusterBoundBuffer = nullptr;
	
	buffer* cmdConstBuffer = nullptr;

	bool debugCamMode = false;

	uint curVertexOffset = 0;
	uint curLodOffset = 0;
	uint curClusterOffset = 0;

	buffer* AABBwireframeBuffer[3];
	buffer* triangleBuffer = nullptr;
	buffer* sceneTriangleBuffer = nullptr;

	buffer* uploadBuffer = nullptr;

private:
	struct cullPassStats
	{
		uint clusterCandidates = 0;
		uint clusterFrustumCulled = 0;
		uint clusterOccluded = 0;
		uint clusterSurvivors = 0;
		uint triCandidates = 0;
		uint triSurvivors = 0;
	};

	struct cullStats
	{
		cullPassStats pass1{};
		cullPassStats pass2{};
		uint instancesTotal = 0;
		uint instancesPass1 = 0;
		uint instancesPass2 = 0;
		bool hzbActive = false;
	};

	static constexpr uint CULLSTATS_HISTORY = 120;

	cullStats cullStatsData;
	std::array<float, CULLSTATS_HISTORY> clusterSurvivorHistory{};
	std::array<float, CULLSTATS_HISTORY> triSurvivorHistory{};
	uint cullStatsHistoryHead = 0;

public:
#if ENGINE_DEBUG_MESH
	framebuffer* getDebugFrameBuffer() const;
	void debugFrameBufferRequest(uint debugMeshID, buffer* projBuffer);
#endif // #if ENGINE_DEBUG_MESH

	void guiSetting();
	void guiGBufferSetting();
	void guiCullingSetting();
	void guiHZBSetting();
	void transitionHZBForGui(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);

#if ENGINE_DEBUG_RESOURCEVIEW
	bool getVideoMemoryInfo(DXGI_MEMORY_SEGMENT_GROUP group, UINT64& budget, UINT64& currentUsage) const;
#endif // ENGINE_DEBUG_RESOURCEVIEW

private:
	framebuffer* getFrameBuffer() const;
	void guiCullingToggles();
	const cullStats& getCullStats() const;
	const float* getClusterSurvivorHistory() const;
	const float* getTriSurvivorHistory() const;
	uint getCullStatsHistoryOffset() const;
	uint getHZBMipCount() const;
	D3D12_GPU_DESCRIPTOR_HANDLE getHZBMipHandle(uint mip) const;
	void getHZBMipSize(uint mip, uint& w, uint& h) const;

public:
	render::UBManager* ubManager;
	void uploadMeshToUB(buffer* vertex, buffer* norm, buffer* index, meshData* meshdata, uint meshID, uint flags);

	void setVertexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, uint slot, buffer* buf);
	void setIndexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, buffer* buf);

	void copyGPUBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, buffer* dst, uint dstOffset, buffer* src, uint srcOffset, uint size);
	void uploadGPUBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, buffer* dst, uint dstOffset, void* data, uint size);
	void uploadCopyGPUBuffer(ID3D12Resource* resource, void* data, uint size);
private:
	void setUpTerrain();
};

extern renderer e_globRenderer;
extern renderBuf e_globGPUBuffer;