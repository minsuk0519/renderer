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

class renderer
{
public:
	bool init(Microsoft::WRL::ComPtr<IDXGIFactory4> dxFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter);
	void draw(float dt);
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

	framebuffer* debugFB;
	bool debugFBRequest = false;
	uint debugFBMeshID;
	UINT64 debugProjection;

	imagebuffer* ssaoTex;
	descriptor ssaoDesc;

public:
	framebuffer* getFrameBuffer() const;
	framebuffer* getDebugFrameBuffer() const;

	void debugFrameBufferRequest(uint debugMeshID, UINT64 ptr);
};

extern renderer e_globRenderer;