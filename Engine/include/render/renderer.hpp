#pragma once
#include <array>
#include <system/defines.hpp>
#include <render/descriptorheap.hpp>
#include <render/buffer.hpp>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>

#include <DirectXMath.h>

class framebuffer;

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
private:
	bool createDevice(Microsoft::WRL::ComPtr<IDXGIFactory4> dxFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter);
	bool checkFeatureSupport(DXGI_FEATURE feature);
	bool createSwapChain();
	bool createFrameResources();

private:
	//created from engine
	Microsoft::WRL::ComPtr<IDXGIFactory4> factory;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;

	Microsoft::WRL::ComPtr<ID3D12CommandSignature> cmdSignature;
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> cmdprepassSignature;

private:
	framebuffer* swapchainFB[FRAME_COUNT];
	framebuffer* gbufferFB;
#if ENGINE_DEBUG_DEBUGCAM
	framebuffer* gbufferDebugFB;
#endif // #if ENGINE_DEBUG_DEBUGCAM

	framebuffer* debugFB;
	bool debugFBRequest = false;
	uint debugFBMeshID;
	UINT64 debugProjection;

	imagebuffer* ssaoTex[3];

	uavbuffer* terrainTex[3];
	uavbuffer* unifiedBuffer[2];
	uavbuffer* commandBuffer;

	meshInfo* meshInfos;
	imagebuffer* meshInfoBuffer;
	imagebuffer* lodInfoBuffer;
	imagebuffer* clusterInfoBuffer;
	uavbuffer* localClusterOffsetBuffer;
	uavbuffer* localClusterSizeBuffer;
	uavbuffer* clusterArgsBuffer;
#if	ENGINE_DEBUG_BUFFER
	uavbuffer* outDebugBuffer;
	descriptor outDebugDesc;
#endif //#if ENGINE_DEBUG_BUFFER
	imagebuffer* viewInfoBuffer;
	imagebuffer* clusterBoundBuffer;
	
	constantbuffer* cmdConstBuffer;

	vertexbuffer* vertexIDBuffer;
	uavbuffer* vertexIDBufferUAV;

	constantbuffer* objectConstBuffer;

	vertexbuffer* AABBwireframeBuffer;

	bool debugCamMode = false;

	uint curVertexOffset = 0;
	uint curLodOffset = 0;
	uint curClusterOffset = 0;
public:
	framebuffer* getFrameBuffer() const;
	framebuffer* getDebugFrameBuffer() const;

	void debugFrameBufferRequest(uint debugMeshID, UINT64 ptr);

	void guiSetting();

	bool createUB();

	descriptor ssaoDesc[3];
	descriptor terrainDesc[3];
	descriptor unifiedDesc[2];
	descriptor commandDesc;
	descriptor commandConstDesc;
	descriptor vertexIDDesc;
	descriptor objectConstDesc;

	descriptor meshInfoDesc;
	descriptor lodInfoDesc;
	descriptor clusterInfoDesc;
	descriptor localClusterOffsetDesc;
	descriptor localClusterSizeDesc;
	descriptor clusterArgsDesc;
	descriptor viewInfoDesc;
	descriptor clusterBoundDesc;
private:
	void setUpTerrain();
};

extern renderer e_globRenderer;
extern renderBuf e_globGPUBuffer;