#pragma once
#include <array>
#include <system/defines.hpp>
#include <render/descriptorheap.hpp>
#include <render/buffer.hpp>
#include <render/render_UB.hpp>

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

	buffer* ssaoTex[3];

	buffer* commandBuffer;
	buffer* objectConstBuffer;
	buffer* localClusterOffsetBuffer;
	buffer* localClusterSizeBuffer;
	buffer* clusterArgsBuffer;

#if	ENGINE_DEBUG_BUFFER
	buffer* outDebugBuffer;
#endif //#if ENGINE_DEBUG_BUFFER
	buffer* viewInfoBuffer;
	buffer* clusterBoundBuffer;
	
	buffer* cmdConstBuffer;

	buffer* vertexIDBuffer;

	bool debugCamMode = false;

	uint curVertexOffset = 0;
	uint curLodOffset = 0;
	uint curClusterOffset = 0;

	buffer* AABBwireframeBuffer[2];
	buffer* triangleBuffer;
public:
	framebuffer* getFrameBuffer() const;
	framebuffer* getDebugFrameBuffer() const;

	void debugFrameBufferRequest(uint debugMeshID, UINT64 ptr);

	void guiSetting();

	render::UBManager* ubManager;
	void uploadMeshToUB(buffer* vertex, buffer* norm, buffer* index, meshData* meshdata, uint meshID);

	void setVertexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, uint slot, buffer* buf);
	void setIndexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList, buffer* buf);
private:
	void setUpTerrain();
};

extern renderer e_globRenderer;
extern renderBuf e_globGPUBuffer;